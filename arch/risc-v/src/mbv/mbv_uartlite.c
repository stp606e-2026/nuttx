/****************************************************************************
 * arch/risc-v/src/mbv/mbv_uartlite.c
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
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/serial/serial.h>

#include "riscv_internal.h"
#include "chip.h"
#include "mbv_uartlite.h"
#include "hardware/mbv_axi_uartlite.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The interrupt is a one clock pulse: the AXI INTC must latch it as an
 * edge.
 */

#if (CONFIG_MBV_INTC_EDGE_MASK & (1 << CONFIG_MBV_UARTLITE0_IRQ)) == 0
#  error "The AXI UART Lite input must be set in CONFIG_MBV_INTC_EDGE_MASK"
#endif

/* The 16550 driver names its first port /dev/ttyS0 as well */

#ifdef CONFIG_16550_UART
#  define UARTLITE0_DEVNAME "/dev/ttyUL0"
#else
#  define UARTLITE0_DEVNAME "/dev/ttyS0"
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mbv_uartlite_s
{
  uintptr_t base;    /* AXI UART Lite base address */
  int       irq;     /* NuttX IRQ number */
  bool      rxint;   /* RX interrupt enabled by the upper half */
  bool      txint;   /* TX interrupt enabled by the upper half */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  mbv_uartlite_setup(struct uart_dev_s *dev);
static void mbv_uartlite_shutdown(struct uart_dev_s *dev);
static int  mbv_uartlite_attach(struct uart_dev_s *dev);
static void mbv_uartlite_detach(struct uart_dev_s *dev);
static int  mbv_uartlite_interrupt(int irq, void *context, void *arg);
static int  mbv_uartlite_ioctl(struct file *filep, int cmd,
                               unsigned long arg);
static int  mbv_uartlite_receive(struct uart_dev_s *dev,
                                 unsigned int *status);
static void mbv_uartlite_rxint(struct uart_dev_s *dev, bool enable);
static bool mbv_uartlite_rxavailable(struct uart_dev_s *dev);
static void mbv_uartlite_send(struct uart_dev_s *dev, int ch);
static void mbv_uartlite_txint(struct uart_dev_s *dev, bool enable);
static bool mbv_uartlite_txready(struct uart_dev_s *dev);
static bool mbv_uartlite_txempty(struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_mbv_uartlite_ops =
{
  .setup       = mbv_uartlite_setup,
  .shutdown    = mbv_uartlite_shutdown,
  .attach      = mbv_uartlite_attach,
  .detach      = mbv_uartlite_detach,
  .ioctl       = mbv_uartlite_ioctl,
  .receive     = mbv_uartlite_receive,
  .rxint       = mbv_uartlite_rxint,
  .rxavailable = mbv_uartlite_rxavailable,
  .send        = mbv_uartlite_send,
  .txint       = mbv_uartlite_txint,
  .txready     = mbv_uartlite_txready,
  .txempty     = mbv_uartlite_txempty,
};

static char g_uartlite0_rxbuffer[CONFIG_MBV_UARTLITE0_RXBUFSIZE];
static char g_uartlite0_txbuffer[CONFIG_MBV_UARTLITE0_TXBUFSIZE];

static struct mbv_uartlite_s g_uartlite0_priv =
{
  .base = CONFIG_MBV_UARTLITE0_BASE,
  .irq  = MBV_IRQ_INTC(CONFIG_MBV_UARTLITE0_IRQ),
};

static struct uart_dev_s g_uartlite0_port =
{
  .recv =
  {
    .size   = CONFIG_MBV_UARTLITE0_RXBUFSIZE,
    .buffer = g_uartlite0_rxbuffer,
  },
  .xmit =
  {
    .size   = CONFIG_MBV_UARTLITE0_TXBUFSIZE,
    .buffer = g_uartlite0_txbuffer,
  },
  .ops  = &g_mbv_uartlite_ops,
  .priv = &g_uartlite0_priv,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_uartlite_getreg / mbv_uartlite_putreg
 ****************************************************************************/

static inline uint32_t mbv_uartlite_getreg(struct mbv_uartlite_s *priv,
                                           unsigned int offset)
{
  return getreg32(priv->base + offset);
}

static inline void mbv_uartlite_putreg(struct mbv_uartlite_s *priv,
                                       unsigned int offset, uint32_t value)
{
  putreg32(value, priv->base + offset);
}

/****************************************************************************
 * Name: mbv_uartlite_setup
 *
 * Description:
 *   Called the first time the port is opened, or early in the boot
 *   sequence for the console.  Baud rate and frame format are fixed in
 *   hardware, so only the RX FIFO is flushed.  The TX FIFO is left alone,
 *   it may still hold boot messages written by up_putc().
 *
 ****************************************************************************/

static int mbv_uartlite_setup(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;

  mbv_uartlite_putreg(priv, AXI_UARTLITE_CTRL_OFFSET,
                      AXI_UARTLITE_CTRL_RSTRX);
  return OK;
}

/****************************************************************************
 * Name: mbv_uartlite_shutdown
 ****************************************************************************/

static void mbv_uartlite_shutdown(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;

  priv->rxint = false;
  priv->txint = false;
  mbv_uartlite_putreg(priv, AXI_UARTLITE_CTRL_OFFSET, 0);
}

/****************************************************************************
 * Name: mbv_uartlite_attach
 *
 * Description:
 *   Attach and enable the interrupt.  The core has a single interrupt
 *   enable for RX and TX, so it is enabled here and the rxint()/txint()
 *   state is kept in software.
 *
 ****************************************************************************/

static int mbv_uartlite_attach(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;
  int ret;

  ret = irq_attach(priv->irq, mbv_uartlite_interrupt, dev);
  if (ret == OK)
    {
      mbv_uartlite_putreg(priv, AXI_UARTLITE_CTRL_OFFSET,
                          AXI_UARTLITE_CTRL_INTREN);
      up_enable_irq(priv->irq);
    }

  return ret;
}

/****************************************************************************
 * Name: mbv_uartlite_detach
 ****************************************************************************/

static void mbv_uartlite_detach(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;

  up_disable_irq(priv->irq);
  mbv_uartlite_putreg(priv, AXI_UARTLITE_CTRL_OFFSET, 0);
  irq_detach(priv->irq);
}

/****************************************************************************
 * Name: mbv_uartlite_interrupt
 *
 * Description:
 *   The core pulses its interrupt when the RX FIFO becomes non-empty and
 *   when the TX FIFO becomes empty.  There is no pending status: the RX
 *   FIFO must be drained completely, otherwise no further RX interrupt
 *   arrives.  The AXI INTC has already acknowledged the edge, so a new
 *   pulse while this runs is kept pending.
 *
 ****************************************************************************/

static int mbv_uartlite_interrupt(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = arg;
  struct mbv_uartlite_s *priv;

  DEBUGASSERT(dev != NULL && dev->priv != NULL);
  priv = dev->priv;

  if (priv->rxint)
    {
      uart_recvchars(dev);
    }

  if (priv->txint)
    {
      uart_xmitchars(dev);
    }

  return OK;
}

/****************************************************************************
 * Name: mbv_uartlite_ioctl
 *
 * Description:
 *   Baud rate and frame format are set in Vivado.  TCGETS reports them,
 *   hardware changes requested with TCSETS are ignored and the TTY flags
 *   are left to the upper half.
 *
 ****************************************************************************/

static int mbv_uartlite_ioctl(struct file *filep, int cmd,
                              unsigned long arg)
{
#ifdef CONFIG_SERIAL_TERMIOS
  struct termios *termiosp = (struct termios *)(uintptr_t)arg;

  if (cmd == TCGETS)
    {
      if (termiosp == NULL)
        {
          return -EINVAL;
        }

      termiosp->c_cflag = CREAD | CLOCAL;

#  if CONFIG_MBV_UARTLITE0_BITS == 5
      termiosp->c_cflag |= CS5;
#  elif CONFIG_MBV_UARTLITE0_BITS == 6
      termiosp->c_cflag |= CS6;
#  elif CONFIG_MBV_UARTLITE0_BITS == 7
      termiosp->c_cflag |= CS7;
#  else
      termiosp->c_cflag |= CS8;
#  endif

#  if CONFIG_MBV_UARTLITE0_PARITY == 1
      termiosp->c_cflag |= PARENB | PARODD;
#  elif CONFIG_MBV_UARTLITE0_PARITY == 2
      termiosp->c_cflag |= PARENB;
#  endif

      cfsetispeed(termiosp, CONFIG_MBV_UARTLITE0_BAUD);
      return OK;
    }
#endif

  return -ENOTTY;
}

/****************************************************************************
 * Name: mbv_uartlite_receive
 ****************************************************************************/

static int mbv_uartlite_receive(struct uart_dev_s *dev,
                                unsigned int *status)
{
  struct mbv_uartlite_s *priv = dev->priv;

  *status = 0;
  return mbv_uartlite_getreg(priv, AXI_UARTLITE_RX_OFFSET) & 0xff;
}

/****************************************************************************
 * Name: mbv_uartlite_rxint
 *
 * Description:
 *   Data that arrived while RX was disabled does not cause a new
 *   interrupt, so it is passed to the upper half here.
 *
 ****************************************************************************/

static void mbv_uartlite_rxint(struct uart_dev_s *dev, bool enable)
{
  struct mbv_uartlite_s *priv = dev->priv;
  irqstate_t flags;

  flags = enter_critical_section();

  priv->rxint = enable;
  if (enable && mbv_uartlite_rxavailable(dev))
    {
      uart_recvchars(dev);
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: mbv_uartlite_rxavailable
 ****************************************************************************/

static bool mbv_uartlite_rxavailable(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;

  return (mbv_uartlite_getreg(priv, AXI_UARTLITE_STAT_OFFSET) &
          AXI_UARTLITE_STAT_RXVALID) != 0;
}

/****************************************************************************
 * Name: mbv_uartlite_send
 ****************************************************************************/

static void mbv_uartlite_send(struct uart_dev_s *dev, int ch)
{
  struct mbv_uartlite_s *priv = dev->priv;

  mbv_uartlite_putreg(priv, AXI_UARTLITE_TX_OFFSET, (uint8_t)ch);
}

/****************************************************************************
 * Name: mbv_uartlite_txint
 *
 * Description:
 *   The TX interrupt only comes when the TX FIFO drains, so start the
 *   transfer here by filling the FIFO.
 *
 ****************************************************************************/

static void mbv_uartlite_txint(struct uart_dev_s *dev, bool enable)
{
  struct mbv_uartlite_s *priv = dev->priv;
  irqstate_t flags;

  flags = enter_critical_section();

  priv->txint = enable;
  if (enable)
    {
      uart_xmitchars(dev);
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: mbv_uartlite_txready
 ****************************************************************************/

static bool mbv_uartlite_txready(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;

  return (mbv_uartlite_getreg(priv, AXI_UARTLITE_STAT_OFFSET) &
          AXI_UARTLITE_STAT_TXFULL) == 0;
}

/****************************************************************************
 * Name: mbv_uartlite_txempty
 ****************************************************************************/

static bool mbv_uartlite_txempty(struct uart_dev_s *dev)
{
  struct mbv_uartlite_s *priv = dev->priv;

  return (mbv_uartlite_getreg(priv, AXI_UARTLITE_STAT_OFFSET) &
          AXI_UARTLITE_STAT_TXEMPTY) != 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_uartlite_earlyserialinit
 ****************************************************************************/

void mbv_uartlite_earlyserialinit(void)
{
#ifdef CONFIG_MBV_UARTLITE0_SERIAL_CONSOLE
  g_uartlite0_port.isconsole = true;
  mbv_uartlite_setup(&g_uartlite0_port);
#endif
}

/****************************************************************************
 * Name: mbv_uartlite_serialinit
 ****************************************************************************/

void mbv_uartlite_serialinit(void)
{
#ifdef CONFIG_MBV_UARTLITE0_SERIAL_CONSOLE
  uart_register("/dev/console", &g_uartlite0_port);
#endif
  uart_register(UARTLITE0_DEVNAME, &g_uartlite0_port);
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Polled console output for the OS debug writes, usable before the
 *   serial driver and interrupts are initialized.
 *
 ****************************************************************************/

#ifdef CONFIG_MBV_UARTLITE0_SERIAL_CONSOLE
void up_putc(int ch)
{
  struct mbv_uartlite_s *priv = &g_uartlite0_priv;

  while ((mbv_uartlite_getreg(priv, AXI_UARTLITE_STAT_OFFSET) &
          AXI_UARTLITE_STAT_TXFULL) != 0)
    {
    }

  mbv_uartlite_putreg(priv, AXI_UARTLITE_TX_OFFSET, (uint8_t)ch);
}
#endif
