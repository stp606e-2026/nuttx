/****************************************************************************
 * arch/risc-v/src/mbv/mbv_ethlite.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/net/ethernet.h>
#include <nuttx/net/netdev_lowerhalf.h>
#include <nuttx/wqueue.h>

#include "riscv_internal.h"
#include "chip.h"
#include "mbv_ethlite.h"
#include "hardware/mbv_axi_ethlite.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_MBV_ETHLITE_TX_PINGPONG
#  define ETHLITE_NTXBUF  2
#else
#  define ETHLITE_NTXBUF  1
#endif

#ifdef CONFIG_MBV_ETHLITE_RX_PINGPONG
#  define ETHLITE_NRXBUF  2
#else
#  define ETHLITE_NRXBUF  1
#endif

/* Packets held by the driver: one per TX buffer until it is sent, and the
 * RX packet being handed to the network stack.
 */

#define ETHLITE_TX_QUOTA  ETHLITE_NTXBUF
#define ETHLITE_RX_QUOTA  2

/* Largest frame without FCS, the packet buffers do not hold the FCS */

#define ETHLITE_MAXFRAME  (ETH_HDRLEN + 1500)

/* Header sizes used to find the length of a received frame */

#define ETHLITE_IPV4_HDRLEN  20
#define ETHLITE_IPV6_HDRLEN  40
#define ETHLITE_ARP_LEN      28

/* Polls for the MAC address load to complete (immediate on QEMU) */

#define ETHLITE_PROGRAM_TIMEOUT  100000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mbv_ethlite_s
{
  struct netdev_lowerhalf_s dev;           /* Lower half, must be first */
  uintptr_t base;                          /* AXI EthernetLite base */
  int       irq;                           /* NuttX IRQ number */
  uint8_t   txnext;                        /* Next TX buffer to fill */
  uint8_t   rxnext;                        /* Next RX buffer to check */
  netpkt_t *txpkt[ETHLITE_NTXBUF];         /* Packet in each TX buffer */

  /* The packet buffers are accessed with 32-bit words only.  Frames are
   * copied through these to and from the network packets.
   */

  uint32_t  txbuf[ETHLITE_MAXFRAME / 4 + 1];
  uint32_t  rxbuf[ETHLITE_MAXFRAME / 4 + 1];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int       mbv_ethlite_ifup(struct netdev_lowerhalf_s *dev);
static int       mbv_ethlite_ifdown(struct netdev_lowerhalf_s *dev);
static int       mbv_ethlite_transmit(struct netdev_lowerhalf_s *dev,
                                      netpkt_t *pkt);
static netpkt_t *mbv_ethlite_receive(struct netdev_lowerhalf_s *dev);
static void      mbv_ethlite_reclaim(struct netdev_lowerhalf_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct netdev_ops_s g_mbv_ethlite_ops =
{
  .ifup     = mbv_ethlite_ifup,
  .ifdown   = mbv_ethlite_ifdown,
  .transmit = mbv_ethlite_transmit,
  .receive  = mbv_ethlite_receive,
  .reclaim  = mbv_ethlite_reclaim,
};

static struct mbv_ethlite_s g_mbv_ethlite0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t mbv_ethlite_getreg(struct mbv_ethlite_s *priv,
                                          unsigned int offset)
{
  return getreg32(priv->base + offset);
}

static inline void mbv_ethlite_putreg(struct mbv_ethlite_s *priv,
                                      unsigned int offset, uint32_t value)
{
  putreg32(value, priv->base + offset);
}

/* Offset of the ping (0) or pong (1) copy of a buffer or register */

static inline unsigned int mbv_ethlite_port(unsigned int offset,
                                            unsigned int index)
{
  return offset + index * AXI_ETHLITE_PONG_OFFSET;
}

/****************************************************************************
 * Name: mbv_ethlite_rxlen
 *
 * Description:
 *   The core has no RX length register, the length of a received frame is
 *   taken from its headers.  Unknown frame types use the maximum length.
 *
 ****************************************************************************/

static unsigned int mbv_ethlite_rxlen(const uint8_t *frame)
{
  unsigned int type = ((unsigned int)frame[12] << 8) | frame[13];
  unsigned int len;

  switch (type)
    {
      case ETHTYPE_IP:
        len = ETH_HDRLEN +
              (((unsigned int)frame[16] << 8) | frame[17]);
        break;

      case ETHTYPE_IP6:
        len = ETH_HDRLEN + ETHLITE_IPV6_HDRLEN +
              (((unsigned int)frame[18] << 8) | frame[19]);
        break;

      case ETHTYPE_ARP:
        len = ETH_HDRLEN + ETHLITE_ARP_LEN;
        break;

      default:

        /* IEEE 802.3 frames carry their length in the type field */

        len = type <= 1500 ? ETH_HDRLEN + type : ETHLITE_MAXFRAME;
        break;
    }

  if (len < ETH_HDRLEN || len > ETHLITE_MAXFRAME)
    {
      len = ETHLITE_MAXFRAME;
    }

  return len;
}

/****************************************************************************
 * Name: mbv_ethlite_setmac
 *
 * Description:
 *   Load the MAC address of the interface into the core.  The core drops
 *   received unicast frames for other addresses.
 *
 ****************************************************************************/

static int mbv_ethlite_setmac(struct mbv_ethlite_s *priv)
{
  const uint8_t *mac = priv->dev.netdev.d_mac.ether.ether_addr_octet;
  int timeout;

  mbv_ethlite_putreg(priv, AXI_ETHLITE_TXBUF_OFFSET,
                     mac[0] | mac[1] << 8 | mac[2] << 16 |
                     (uint32_t)mac[3] << 24);
  mbv_ethlite_putreg(priv, AXI_ETHLITE_TXBUF_OFFSET + 4,
                     mac[4] | mac[5] << 8);
  mbv_ethlite_putreg(priv, AXI_ETHLITE_TXCTRL_OFFSET,
                     AXI_ETHLITE_TXCTRL_PROGRAM | AXI_ETHLITE_TXCTRL_STATUS);

  for (timeout = 0; timeout < ETHLITE_PROGRAM_TIMEOUT; timeout++)
    {
      if ((mbv_ethlite_getreg(priv, AXI_ETHLITE_TXCTRL_OFFSET) &
           AXI_ETHLITE_TXCTRL_STATUS) == 0)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: mbv_ethlite_interrupt
 *
 * Description:
 *   The core raises one interrupt for a received frame and for a completed
 *   transmission.  Hand both to the upper half, which receives and reclaims
 *   in its work queue.  Requesting a receive on every interrupt makes sure
 *   a frame that arrives while this handler runs is not missed.
 *
 ****************************************************************************/

static int mbv_ethlite_interrupt(int irq, void *context, void *arg)
{
  struct mbv_ethlite_s *priv = arg;
  int i;

  netdev_lower_rxready(&priv->dev);

  for (i = 0; i < ETHLITE_NTXBUF; i++)
    {
      if (priv->txpkt[i] != NULL)
        {
          netdev_lower_txdone(&priv->dev);
          break;
        }
    }

  return OK;
}

/****************************************************************************
 * Name: mbv_ethlite_reset
 *
 * Description:
 *   Disable the interrupts, drop any received frame and release the
 *   packets still held in the TX buffers.
 *
 ****************************************************************************/

static void mbv_ethlite_reset(struct mbv_ethlite_s *priv)
{
  int i;

  mbv_ethlite_putreg(priv, AXI_ETHLITE_GIE_OFFSET, 0);

  for (i = 0; i < ETHLITE_NTXBUF; i++)
    {
      mbv_ethlite_putreg(priv,
                         mbv_ethlite_port(AXI_ETHLITE_TXCTRL_OFFSET, i), 0);

      if (priv->txpkt[i] != NULL)
        {
          netpkt_free(&priv->dev, priv->txpkt[i], NETPKT_TX);
          priv->txpkt[i] = NULL;
        }
    }

  for (i = 0; i < ETHLITE_NRXBUF; i++)
    {
      mbv_ethlite_putreg(priv,
                         mbv_ethlite_port(AXI_ETHLITE_RXCTRL_OFFSET, i), 0);
    }

  priv->txnext = 0;
  priv->rxnext = 0;
}

/****************************************************************************
 * Name: mbv_ethlite_ifup
 *
 * Description:
 *   Bring the interface up: load the MAC address and enable the RX and
 *   TX done interrupts.  The core has no PHY management in this driver,
 *   the carrier is reported as present.
 *
 ****************************************************************************/

static int mbv_ethlite_ifup(struct netdev_lowerhalf_s *dev)
{
  struct mbv_ethlite_s *priv = (struct mbv_ethlite_s *)dev;
  int ret;
  int i;

  mbv_ethlite_reset(priv);

  ret = mbv_ethlite_setmac(priv);
  if (ret < 0)
    {
      nerr("ERROR: Failed to load the MAC address: %d\n", ret);
      return ret;
    }

  /* The interrupt enable bit is only implemented in the ping registers,
   * setting it in the pong ones is harmless.  QEMU checks the registers of
   * the buffer in use.
   */

  for (i = 0; i < ETHLITE_NTXBUF; i++)
    {
      mbv_ethlite_putreg(priv,
                         mbv_ethlite_port(AXI_ETHLITE_TXCTRL_OFFSET, i),
                         AXI_ETHLITE_TXCTRL_IE);
    }

  for (i = 0; i < ETHLITE_NRXBUF; i++)
    {
      mbv_ethlite_putreg(priv,
                         mbv_ethlite_port(AXI_ETHLITE_RXCTRL_OFFSET, i),
                         AXI_ETHLITE_RXCTRL_IE);
    }

  mbv_ethlite_putreg(priv, AXI_ETHLITE_GIE_OFFSET, AXI_ETHLITE_GIE_GIE);
  up_enable_irq(priv->irq);

  netdev_lower_carrier_on(dev);
  return OK;
}

/****************************************************************************
 * Name: mbv_ethlite_ifdown
 ****************************************************************************/

static int mbv_ethlite_ifdown(struct netdev_lowerhalf_s *dev)
{
  struct mbv_ethlite_s *priv = (struct mbv_ethlite_s *)dev;
  irqstate_t flags;

  flags = enter_critical_section();
  up_disable_irq(priv->irq);
  mbv_ethlite_reset(priv);
  leave_critical_section(flags);

  netdev_lower_carrier_off(dev);
  return OK;
}

/****************************************************************************
 * Name: mbv_ethlite_reclaim
 *
 * Description:
 *   Release the packets of the TX buffers whose transmission completed.
 *
 ****************************************************************************/

static void mbv_ethlite_reclaim(struct netdev_lowerhalf_s *dev)
{
  struct mbv_ethlite_s *priv = (struct mbv_ethlite_s *)dev;
  unsigned int ctrl;
  int i;

  for (i = 0; i < ETHLITE_NTXBUF; i++)
    {
      ctrl = mbv_ethlite_port(AXI_ETHLITE_TXCTRL_OFFSET, i);
      if (priv->txpkt[i] != NULL &&
          (mbv_ethlite_getreg(priv, ctrl) & AXI_ETHLITE_TXCTRL_STATUS) == 0)
        {
          netpkt_free(dev, priv->txpkt[i], NETPKT_TX);
          priv->txpkt[i] = NULL;
        }
    }
}

/****************************************************************************
 * Name: mbv_ethlite_transmit
 *
 * Description:
 *   Copy a frame into the next free TX buffer and start sending it.  The
 *   packet is kept until the buffer is free again, so that the TX quota
 *   follows the number of TX buffers.
 *
 ****************************************************************************/

static int mbv_ethlite_transmit(struct netdev_lowerhalf_s *dev,
                                netpkt_t *pkt)
{
  struct mbv_ethlite_s *priv = (struct mbv_ethlite_s *)dev;
  unsigned int buf = priv->txnext;
  unsigned int len = netpkt_getdatalen(dev, pkt);
  uintptr_t offset;
  unsigned int i;

  if (len > ETHLITE_MAXFRAME)
    {
      return -EMSGSIZE;
    }

  if (priv->txpkt[buf] != NULL)
    {
      mbv_ethlite_reclaim(dev);
      if (priv->txpkt[buf] != NULL)
        {
          return -EBUSY;
        }
    }

  netpkt_copyout(dev, (uint8_t *)priv->txbuf, pkt, len, 0);

  offset = mbv_ethlite_port(AXI_ETHLITE_TXBUF_OFFSET, buf);
  for (i = 0; i < (len + 3) / 4; i++)
    {
      mbv_ethlite_putreg(priv, offset + i * 4, priv->txbuf[i]);
    }

  priv->txpkt[buf] = pkt;
  priv->txnext = (buf + 1) % ETHLITE_NTXBUF;

  mbv_ethlite_putreg(priv, mbv_ethlite_port(AXI_ETHLITE_TXLEN_OFFSET, buf),
                     len);
  mbv_ethlite_putreg(priv, mbv_ethlite_port(AXI_ETHLITE_TXCTRL_OFFSET, buf),
                     AXI_ETHLITE_TXCTRL_IE | AXI_ETHLITE_TXCTRL_STATUS);

  return OK;
}

/****************************************************************************
 * Name: mbv_ethlite_receive
 *
 * Description:
 *   Return the frame in the next RX buffer, or NULL if it is empty.  The
 *   buffer is handed back to the core as soon as the frame is copied.
 *
 ****************************************************************************/

static netpkt_t *mbv_ethlite_receive(struct netdev_lowerhalf_s *dev)
{
  struct mbv_ethlite_s *priv = (struct mbv_ethlite_s *)dev;
  unsigned int ctrl;
  uintptr_t offset;
  unsigned int len;
  unsigned int i;
  netpkt_t *pkt;

  ctrl = mbv_ethlite_port(AXI_ETHLITE_RXCTRL_OFFSET, priv->rxnext);
  if ((mbv_ethlite_getreg(priv, ctrl) & AXI_ETHLITE_RXCTRL_STATUS) == 0)
    {
      return NULL;
    }

  /* Read the Ethernet header and the start of the IP header first, they
   * give the frame length.
   */

  offset = mbv_ethlite_port(AXI_ETHLITE_RXBUF_OFFSET, priv->rxnext);
  for (i = 0; i < 5; i++)
    {
      priv->rxbuf[i] = mbv_ethlite_getreg(priv, offset + i * 4);
    }

  len = mbv_ethlite_rxlen((const uint8_t *)priv->rxbuf);
  for (; i < (len + 3) / 4; i++)
    {
      priv->rxbuf[i] = mbv_ethlite_getreg(priv, offset + i * 4);
    }

  /* Give the buffer back, this also lets QEMU deliver queued frames */

  mbv_ethlite_putreg(priv, ctrl, AXI_ETHLITE_RXCTRL_IE);
  priv->rxnext = (priv->rxnext + 1) % ETHLITE_NRXBUF;

  pkt = netpkt_alloc(dev, NETPKT_RX);
  if (pkt == NULL)
    {
      NETDEV_RXDROPPED(&dev->netdev);
      return NULL;
    }

  if (netpkt_copyin(dev, pkt, (const uint8_t *)priv->rxbuf, len, 0) < 0)
    {
      NETDEV_RXERRORS(&dev->netdev);
      netpkt_free(dev, pkt, NETPKT_RX);
      return NULL;
    }

  return pkt;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_ethlite_initialize
 *
 * Description:
 *   Initialize the AXI EthernetLite and register it with the network
 *   stack.
 *
 ****************************************************************************/

int mbv_ethlite_initialize(void)
{
  struct mbv_ethlite_s *priv = &g_mbv_ethlite0;
  struct netdev_lowerhalf_s *dev = &priv->dev;
  uint8_t *mac = dev->netdev.d_mac.ether.ether_addr_octet;
  int ret;

  priv->base = CONFIG_MBV_ETHLITE_BASE;
  priv->irq  = MBV_IRQ_INTC(CONFIG_MBV_ETHLITE_IRQ);

  mbv_ethlite_reset(priv);

  mac[0] = (CONFIG_MBV_ETHLITE_MACADDR_1 >> 8) & 0xff;
  mac[1] = CONFIG_MBV_ETHLITE_MACADDR_1 & 0xff;
  mac[2] = (CONFIG_MBV_ETHLITE_MACADDR_2 >> 24) & 0xff;
  mac[3] = (CONFIG_MBV_ETHLITE_MACADDR_2 >> 16) & 0xff;
  mac[4] = (CONFIG_MBV_ETHLITE_MACADDR_2 >> 8) & 0xff;
  mac[5] = CONFIG_MBV_ETHLITE_MACADDR_2 & 0xff;

  ret = irq_attach(priv->irq, mbv_ethlite_interrupt, priv);
  if (ret < 0)
    {
      nerr("ERROR: Failed to attach IRQ %d: %d\n", priv->irq, ret);
      return ret;
    }

  dev->ops                = &g_mbv_ethlite_ops;
  dev->quota[NETPKT_TX]   = ETHLITE_TX_QUOTA;
  dev->quota[NETPKT_RX]   = ETHLITE_RX_QUOTA;
  dev->rxtype             = NETDEV_RX_WORK;
  dev->priority           = LPWORK;

  ret = netdev_lower_register(dev, NET_LL_ETHERNET);
  if (ret < 0)
    {
      nerr("ERROR: Failed to register the network device: %d\n", ret);
      irq_detach(priv->irq);
    }

  return ret;
}

/****************************************************************************
 * Name: riscv_netinitialize
 *
 * Description:
 *   Called from riscv_initialize() to bring up the network interface.
 *
 ****************************************************************************/

#ifndef CONFIG_NETDEV_LATEINIT
void riscv_netinitialize(void)
{
  mbv_ethlite_initialize();
}
#endif
