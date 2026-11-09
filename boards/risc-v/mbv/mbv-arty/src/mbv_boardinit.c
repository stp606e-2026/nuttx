/****************************************************************************
 * boards/risc-v/mbv/mbv-arty/src/mbv_boardinit.c
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

#include <sys/mount.h>
#include <syslog.h>

#include <nuttx/board.h>
#include <nuttx/irq.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mmcsd.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/spi_transfer.h>
#include <arch/board/board.h>

#include "mbv_spi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The image runs either from the LMB BRAM or from the DDR, see
 * scripts/ld.script
 */

#if CONFIG_RAM_START == MBV_BRAM_BASE
#  if CONFIG_RAM_SIZE > MBV_BRAM_SIZE
#    error "CONFIG_RAM_SIZE is larger than the LMB BRAM"
#  endif
#elif CONFIG_RAM_START == MBV_DDR_BASE
#  if CONFIG_RAM_SIZE > MBV_DDR_SIZE
#    error "CONFIG_RAM_SIZE is larger than the DDR"
#  endif
#else
#  error "CONFIG_RAM_START must be the LMB BRAM or the DDR"
#endif

/* The design has no AXI UART16550 or AXI EthernetLite */

#if defined(CONFIG_16550_UART) || defined(CONFIG_MBV_ETHLITE)
#  error "The Arty A7 design has no AXI UART16550 or AXI EthernetLite"
#endif

#ifdef CONFIG_MBV_UARTLITE0
#  if CONFIG_MBV_UARTLITE0_BASE != MBV_UARTLITE_BASE
#    error "CONFIG_MBV_UARTLITE0_BASE does not match the board memory map"
#  endif
#  if CONFIG_MBV_UARTLITE0_IRQ != MBV_INTC_IN_UARTLITE
#    error "CONFIG_MBV_UARTLITE0_IRQ does not match the AXI INTC wiring"
#  endif
#endif

#if CONFIG_MBV_INTC_NIRQS != MBV_INTC_NINPUTS
#  error "CONFIG_MBV_INTC_NIRQS does not match the AXI INTC of the design"
#endif

#if CONFIG_MBV_INTC_EDGE_MASK != MBV_INTC_EDGE_INPUTS
#  error "CONFIG_MBV_INTC_EDGE_MASK does not match C_KIND_OF_INTR"
#endif

/* The MicroBlaze V uses fast interrupts (C_USE_INTERRUPT = 2): without the
 * IVAR setup every external interrupt jumps to 0x10.
 */

#ifndef CONFIG_MBV_INTC_HAS_FAST
#  error "The design uses fast interrupts, enable CONFIG_MBV_INTC_HAS_FAST"
#endif

#ifdef CONFIG_MBV_SPI0
#  if CONFIG_MBV_SPI0_BASE != MBV_QSPI_BASE
#    error "CONFIG_MBV_SPI0_BASE does not match the board memory map"
#  endif
#  if CONFIG_MBV_SPI0_FIFO_DEPTH != MBV_QSPI_FIFO_DEPTH
#    error "CONFIG_MBV_SPI0_FIFO_DEPTH does not match C_FIFO_DEPTH"
#  endif
#  if CONFIG_MBV_SPI0_SCK_FREQUENCY != MBV_QSPI_SCK_FREQUENCY
#    error "CONFIG_MBV_SPI0_SCK_FREQUENCY does not match the fixed SCK"
#  endif
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_sd_initialize
 *
 * Description:
 *   Bind the SD card on AXI Quad SPI slave select 0 (HW-125 module on
 *   Pmod JA) to the MMC/SD SPI driver as /dev/mmcsd0 and, with FAT, mount
 *   it on CONFIG_MBV_ARTY_SD_MOUNTPOINT.
 *
 ****************************************************************************/

#ifdef CONFIG_MMCSD_SPI
static void mbv_sd_initialize(struct spi_dev_s *spi)
{
  int ret;
  int i;

  /* An SD card needs at least 74 clocks after power-up.  The MMC/SD driver
   * sends them with chip select deasserted, but the AXI Quad SPI does not
   * clock SCK while every slave select output is inactive (seen on the
   * board).  Send them with the card selected first; it ignores clocks
   * without a command.
   */

  SPI_LOCK(spi, true);
  SPI_SETMODE(spi, SPIDEV_MODE0);
  SPI_SETBITS(spi, 8);
  SPI_SELECT(spi, SPIDEV_MMCSD(0), true);
  for (i = 0; i < 10; i++)
    {
      SPI_SEND(spi, 0xff);
    }

  SPI_SELECT(spi, SPIDEV_MMCSD(0), false);
  SPI_LOCK(spi, false);

  ret = mmcsd_spislotinitialize(0, 0, spi);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to bind the SD card: %d\n", ret);
      return;
    }

#ifdef CONFIG_FS_FAT
  ret = nx_mount("/dev/mmcsd0", CONFIG_MBV_ARTY_SD_MOUNTPOINT, "vfat", 0,
                 NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount the SD card on %s: %d\n",
             CONFIG_MBV_ARTY_SD_MOUNTPOINT, ret);
    }
  else
    {
      syslog(LOG_INFO, "SD card mounted on %s\n",
             CONFIG_MBV_ARTY_SD_MOUNTPOINT);
    }
#endif
}
#endif /* CONFIG_MMCSD_SPI */

/****************************************************************************
 * Name: mbv_spi_initialize
 *
 * Description:
 *   Initialize the AXI Quad SPI, optionally register it as /dev/spi0 for
 *   the SPI tool, and set up the SD card.
 *
 ****************************************************************************/

#ifdef CONFIG_MBV_SPI0
static void mbv_spi_initialize(void)
{
  struct spi_dev_s *spi;
  int ret;

  UNUSED(ret);

  spi = mbv_spibus_initialize(0);
  if (spi == NULL)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize AXI Quad SPI\n");
      return;
    }

#ifdef CONFIG_SPI_DRIVER
  ret = spi_register(spi, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register /dev/spi0: %d\n", ret);
    }
#endif

#ifdef CONFIG_MMCSD_SPI
  mbv_sd_initialize(spi);
#endif
}
#endif /* CONFIG_MBV_SPI0 */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_late_initialize().  board_late_initialize() will
 *   be called after up_initialize() and board_early_initialize() and just
 *   before the initial application is started.  This is the place to
 *   register board specific drivers.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret;

  UNUSED(ret);

#ifdef CONFIG_FS_PROCFS
  ret = mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount procfs: %d\n", ret);
    }
#endif

#ifdef CONFIG_MBV_SPI0
  mbv_spi_initialize();
#endif
}
#endif
