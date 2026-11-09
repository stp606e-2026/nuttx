/****************************************************************************
 * arch/risc-v/src/mbv/mbv_spi.c
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
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>

#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/spi/spi.h>

#include "riscv_internal.h"
#include "chip.h"
#include "mbv_spi.h"
#include "hardware/mbv_axi_qspi.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mbv_spi_s
{
  struct spi_dev_s dev;     /* Externally visible part of the SPI interface */
  uintptr_t        base;    /* AXI Quad SPI base address */
  uint8_t          nss;     /* Number of slave select outputs */
  uint16_t         fifo;    /* TX/RX FIFO depth */
  uint32_t         sckfreq; /* Fixed SCK frequency */
  uint32_t         spicr;   /* SPICR while idle (MTI set) */
  bool             initialized;
  mutex_t          lock;    /* Held while the bus is locked */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int      mbv_spi_lock(struct spi_dev_s *dev, bool lock);
static void     mbv_spi_select(struct spi_dev_s *dev, uint32_t devid,
                               bool selected);
static uint32_t mbv_spi_setfrequency(struct spi_dev_s *dev,
                                     uint32_t frequency);
static void     mbv_spi_setmode(struct spi_dev_s *dev, enum spi_mode_e mode);
static void     mbv_spi_setbits(struct spi_dev_s *dev, int nbits);
static uint8_t  mbv_spi_status(struct spi_dev_s *dev, uint32_t devid);
static uint32_t mbv_spi_send(struct spi_dev_s *dev, uint32_t wd);
static void     mbv_spi_exchange(struct spi_dev_s *dev,
                                 const void *txbuffer, void *rxbuffer,
                                 size_t nwords);
#ifndef CONFIG_SPI_EXCHANGE
static void     mbv_spi_sndblock(struct spi_dev_s *dev,
                                 const void *buffer, size_t nwords);
static void     mbv_spi_recvblock(struct spi_dev_s *dev, void *buffer,
                                  size_t nwords);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct spi_ops_s g_mbv_spi_ops =
{
  .lock              = mbv_spi_lock,
  .select            = mbv_spi_select,
  .setfrequency      = mbv_spi_setfrequency,
  .setmode           = mbv_spi_setmode,
  .setbits           = mbv_spi_setbits,
  .status            = mbv_spi_status,
  .send              = mbv_spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange          = mbv_spi_exchange,
#else
  .sndblock          = mbv_spi_sndblock,
  .recvblock         = mbv_spi_recvblock,
#endif
};

#ifdef CONFIG_MBV_SPI0
static struct mbv_spi_s g_mbv_spi0 =
{
  .dev     =
  {
    .ops   = &g_mbv_spi_ops,
  },
  .base    = CONFIG_MBV_SPI0_BASE,
  .nss     = CONFIG_MBV_SPI0_NSS,
  .fifo    = CONFIG_MBV_SPI0_FIFO_DEPTH,
  .sckfreq = CONFIG_MBV_SPI0_SCK_FREQUENCY,
  .lock    = NXMUTEX_INITIALIZER,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t mbv_spi_getreg(struct mbv_spi_s *priv,
                                      unsigned int offset)
{
  return getreg32(priv->base + offset);
}

static inline void mbv_spi_putreg(struct mbv_spi_s *priv,
                                  unsigned int offset, uint32_t value)
{
  putreg32(value, priv->base + offset);
}

/****************************************************************************
 * Name: mbv_spi_lock
 *
 * Description:
 *   Lock or unlock the SPI bus for a sequence of transfers to one device.
 *
 ****************************************************************************/

static int mbv_spi_lock(struct spi_dev_s *dev, bool lock)
{
  struct mbv_spi_s *priv = (struct mbv_spi_s *)dev;

  if (lock)
    {
      return nxmutex_lock(&priv->lock);
    }

  return nxmutex_unlock(&priv->lock);
}

/****************************************************************************
 * Name: mbv_spi_select
 *
 * Description:
 *   Assert or deassert the slave select output given by the index of the
 *   device ID.  Manual slave select is enabled (SPICR.MSSAE), so SSR drives
 *   the outputs directly and the device stays selected between transfers.
 *
 ****************************************************************************/

static void mbv_spi_select(struct spi_dev_s *dev, uint32_t devid,
                           bool selected)
{
  struct mbv_spi_s *priv = (struct mbv_spi_s *)dev;
  uint32_t index = SPIDEVID_INDEX(devid);

  if (index >= priv->nss)
    {
      spierr("ERROR: No slave select output for devid %08" PRIx32 "\n",
             devid);
      return;
    }

  spiinfo("devid: %08" PRIx32 " CS: %s\n",
          devid, selected ? "assert" : "de-assert");

  mbv_spi_putreg(priv, AXI_QSPI_SSR_OFFSET,
                 selected ? ~(1u << index) : 0xffffffff);
}

/****************************************************************************
 * Name: mbv_spi_setfrequency
 *
 * Description:
 *   The SCK frequency is fixed by the hardware configuration
 *   (ext_spi_clk / C_SCK_RATIO), so just return it.
 *
 ****************************************************************************/

static uint32_t mbv_spi_setfrequency(struct spi_dev_s *dev,
                                     uint32_t frequency)
{
  struct mbv_spi_s *priv = (struct mbv_spi_s *)dev;

  if (frequency < priv->sckfreq)
    {
      spiwarn("WARNING: %" PRIu32 " Hz requested, SCK is fixed at "
              "%" PRIu32 " Hz\n", frequency, priv->sckfreq);
    }

  return priv->sckfreq;
}

/****************************************************************************
 * Name: mbv_spi_setmode
 *
 * Description:
 *   Set the SPI mode (CPOL and CPHA).
 *
 ****************************************************************************/

static void mbv_spi_setmode(struct spi_dev_s *dev, enum spi_mode_e mode)
{
  struct mbv_spi_s *priv = (struct mbv_spi_s *)dev;
  uint32_t spicr = priv->spicr &
                   ~(AXI_QSPI_SPICR_CPOL | AXI_QSPI_SPICR_CPHA);

  switch (mode)
    {
      case SPIDEV_MODE0:
        break;

      case SPIDEV_MODE1:
        spicr |= AXI_QSPI_SPICR_CPHA;
        break;

      case SPIDEV_MODE2:
        spicr |= AXI_QSPI_SPICR_CPOL;
        break;

      case SPIDEV_MODE3:
        spicr |= AXI_QSPI_SPICR_CPOL | AXI_QSPI_SPICR_CPHA;
        break;

      default:
        spierr("ERROR: Unsupported mode %d\n", mode);
        return;
    }

  if (spicr != priv->spicr)
    {
      priv->spicr = spicr;
      mbv_spi_putreg(priv, AXI_QSPI_SPICR_OFFSET, spicr);
    }
}

/****************************************************************************
 * Name: mbv_spi_setbits
 *
 * Description:
 *   The transfer width is fixed in hardware (C_NUM_TRANSFER_BITS), only 8
 *   bit transfers are supported.
 *
 ****************************************************************************/

static void mbv_spi_setbits(struct spi_dev_s *dev, int nbits)
{
  if (nbits != 8)
    {
      spierr("ERROR: %d bit transfers are not supported\n", nbits);
      DEBUGPANIC();
    }
}

/****************************************************************************
 * Name: mbv_spi_status
 *
 * Description:
 *   The AXI Quad SPI has no card detect or write protect inputs.  Report an
 *   MMC/SD card as present so that the MMC/SD driver probes it; if there is
 *   no card, it gets no answer to CMD0 and reports no media.
 *
 ****************************************************************************/

static uint8_t mbv_spi_status(struct spi_dev_s *dev, uint32_t devid)
{
  if (SPIDEVID_TYPE(devid) == SPIDEVTYPE_MMCSD)
    {
      return SPI_STATUS_PRESENT;
    }

  return 0;
}

/****************************************************************************
 * Name: mbv_spi_send
 *
 * Description:
 *   Exchange one word on the SPI bus.
 *
 ****************************************************************************/

static uint32_t mbv_spi_send(struct spi_dev_s *dev, uint32_t wd)
{
  uint8_t txbyte = (uint8_t)wd;
  uint8_t rxbyte;

  mbv_spi_exchange(dev, &txbyte, &rxbyte, 1);
  return rxbyte;
}

/****************************************************************************
 * Name: mbv_spi_exchange
 *
 * Description:
 *   Exchange a block of data.  The transfer is split into chunks of at
 *   most one FIFO: the TX FIFO is filled while transfers are inhibited
 *   (SPICR.MTI), the transfer is started by clearing MTI and every TX byte
 *   clocks one byte into the RX FIFO, which is drained before the next
 *   chunk.  0xff is sent if txbuffer is NULL, received data is discarded
 *   if rxbuffer is NULL.
 *
 ****************************************************************************/

static void mbv_spi_exchange(struct spi_dev_s *dev,
                             const void *txbuffer, void *rxbuffer,
                             size_t nwords)
{
  struct mbv_spi_s *priv = (struct mbv_spi_s *)dev;
  const uint8_t *tx = txbuffer;
  uint8_t *rx = rxbuffer;
  uint32_t data;
  size_t chunk;
  size_t i;

  spiinfo("txbuffer=%p rxbuffer=%p nwords=%zu\n",
          txbuffer, rxbuffer, nwords);

  while (nwords > 0)
    {
      chunk = MIN(nwords, priv->fifo);

      for (i = 0; i < chunk; i++)
        {
          mbv_spi_putreg(priv, AXI_QSPI_DTR_OFFSET, tx ? tx[i] : 0xff);
        }

      mbv_spi_putreg(priv, AXI_QSPI_SPICR_OFFSET,
                     priv->spicr & ~AXI_QSPI_SPICR_MTI);

      for (i = 0; i < chunk; i++)
        {
          while ((mbv_spi_getreg(priv, AXI_QSPI_SPISR_OFFSET) &
                  AXI_QSPI_SPISR_RXEMPTY) != 0)
            {
            }

          data = mbv_spi_getreg(priv, AXI_QSPI_DRR_OFFSET);
          if (rx)
            {
              rx[i] = (uint8_t)data;
            }
        }

      mbv_spi_putreg(priv, AXI_QSPI_SPICR_OFFSET, priv->spicr);

      if (tx)
        {
          tx += chunk;
        }

      if (rx)
        {
          rx += chunk;
        }

      nwords -= chunk;
    }
}

#ifndef CONFIG_SPI_EXCHANGE
/****************************************************************************
 * Name: mbv_spi_sndblock
 ****************************************************************************/

static void mbv_spi_sndblock(struct spi_dev_s *dev,
                             const void *buffer, size_t nwords)
{
  mbv_spi_exchange(dev, buffer, NULL, nwords);
}

/****************************************************************************
 * Name: mbv_spi_recvblock
 ****************************************************************************/

static void mbv_spi_recvblock(struct spi_dev_s *dev, void *buffer,
                              size_t nwords)
{
  mbv_spi_exchange(dev, NULL, buffer, nwords);
}
#endif

/****************************************************************************
 * Name: mbv_spi_hwinit
 *
 * Description:
 *   Reset the AXI Quad SPI and configure it as a master in mode 0 with
 *   manual slave select, all slaves deselected, interrupts disabled and
 *   transfers inhibited.
 *
 ****************************************************************************/

static void mbv_spi_hwinit(struct mbv_spi_s *priv)
{
  mbv_spi_putreg(priv, AXI_QSPI_SRR_OFFSET, AXI_QSPI_SRR_RESET);

  priv->spicr = AXI_QSPI_SPICR_SPE | AXI_QSPI_SPICR_MASTER |
                AXI_QSPI_SPICR_MSSAE | AXI_QSPI_SPICR_MTI;

  mbv_spi_putreg(priv, AXI_QSPI_SSR_OFFSET, 0xffffffff);
  mbv_spi_putreg(priv, AXI_QSPI_DGIER_OFFSET, 0);
  mbv_spi_putreg(priv, AXI_QSPI_SPICR_OFFSET,
                 priv->spicr | AXI_QSPI_SPICR_TXRST | AXI_QSPI_SPICR_RXRST);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_spibus_initialize
 *
 * Description:
 *   Initialize the selected AXI Quad SPI bus.
 *
 * Input Parameters:
 *   bus - The SPI bus number (0 for MBV_SPI0)
 *
 * Returned Value:
 *   A valid SPI device structure reference on success; NULL on failure.
 *
 ****************************************************************************/

struct spi_dev_s *mbv_spibus_initialize(int bus)
{
  struct mbv_spi_s *priv;

  switch (bus)
    {
#ifdef CONFIG_MBV_SPI0
      case 0:
        priv = &g_mbv_spi0;
        break;
#endif

      default:
        spierr("ERROR: No AXI Quad SPI %d\n", bus);
        return NULL;
    }

  nxmutex_lock(&priv->lock);
  if (!priv->initialized)
    {
      mbv_spi_hwinit(priv);
      priv->initialized = true;
    }

  nxmutex_unlock(&priv->lock);
  return &priv->dev;
}
