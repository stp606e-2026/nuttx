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
#include <sys/types.h>
#include <syslog.h>

#include <nuttx/board.h>
#include <nuttx/irq.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/mtd/mtd.h>
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
#  if CONFIG_RAM_SIZE > 0x40000000
#    error "CONFIG_RAM_SIZE is larger than the LMB BRAM address space"
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

#ifdef CONFIG_MBV_SPI0
#  if CONFIG_MBV_SPI0_BASE != MBV_QSPI_BASE
#    error "CONFIG_MBV_SPI0_BASE does not match the board memory map"
#  endif
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_flash_initialize
 *
 * Description:
 *   Bind the configuration flash on AXI Quad SPI slave select 0 to the
 *   M25P MTD driver.  The bitstream sits at the start of the flash, so
 *   only the partition from CONFIG_MBV_ARTY_FLASH_FS_OFFSET to the end is
 *   registered, as /dev/mtd0.  With LittleFS, mount it on /mnt/flash and
 *   format it if it does not contain a file system yet.
 *
 ****************************************************************************/

#ifdef CONFIG_MTD_M25P
static void mbv_flash_initialize(struct spi_dev_s *spi)
{
  struct mtd_geometry_s geo;
  struct mtd_dev_s *flash;
  struct mtd_dev_s *part;
  off_t firstblock;
  off_t nblocks;
  int ret;

  flash = m25p_initialize(spi);
  if (flash == NULL)
    {
      syslog(LOG_ERR, "ERROR: No M25P flash on AXI Quad SPI SS0\n");
      return;
    }

  ret = MTD_IOCTL(flash, MTDIOC_GEOMETRY, (unsigned long)&geo);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Flash geometry: %d\n", ret);
      return;
    }

  if (CONFIG_MBV_ARTY_FLASH_FS_OFFSET % geo.erasesize != 0 ||
      CONFIG_MBV_ARTY_FLASH_FS_OFFSET >=
      (uint64_t)geo.erasesize * geo.neraseblocks)
    {
      syslog(LOG_ERR, "ERROR: Bad file system offset 0x%x\n",
             CONFIG_MBV_ARTY_FLASH_FS_OFFSET);
      return;
    }

  firstblock = CONFIG_MBV_ARTY_FLASH_FS_OFFSET / geo.blocksize;
  nblocks    = (off_t)geo.erasesize / geo.blocksize * geo.neraseblocks -
               firstblock;

  part = mtd_partition(flash, firstblock, nblocks);
  if (part == NULL)
    {
      syslog(LOG_ERR, "ERROR: Failed to create the flash partition\n");
      return;
    }

  ret = register_mtddriver("/dev/mtd0", part, 0755, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register /dev/mtd0: %d\n", ret);
      return;
    }

#ifdef CONFIG_FS_LITTLEFS
  ret = nx_mount("/dev/mtd0", "/mnt/flash", "littlefs", 0, "autoformat");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount /mnt/flash: %d\n", ret);
    }
#endif
}
#endif /* CONFIG_MTD_M25P */

/****************************************************************************
 * Name: mbv_spi_initialize
 *
 * Description:
 *   Initialize the AXI Quad SPI, optionally register it as /dev/spi0 for
 *   the SPI tool, and set up the configuration flash.
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

#ifdef CONFIG_MTD_M25P
  mbv_flash_initialize(spi);
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
