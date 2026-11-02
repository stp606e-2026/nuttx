/****************************************************************************
 * boards/risc-v/mbv/mbv-qemu/src/mbv_boardinit.c
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
#include <nuttx/mtd/mtd.h>
#include <nuttx/spi/spi_transfer.h>
#include <arch/board/board.h>

#include "mbv_spi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The 16550 driver takes plain numbers from Kconfig, make sure they match
 * the AXI INTC wiring of this board.
 */

#ifdef CONFIG_16550_UART0
#  if CONFIG_16550_UART0_IRQ != MBV_IRQ_UART16550
#    error "CONFIG_16550_UART0_IRQ must be RISCV_IRQ_EXT + AXI INTC input"
#  endif
#  if CONFIG_16550_UART0_BASE != MBV_UART16550_BASE
#    error "CONFIG_16550_UART0_BASE does not match the board memory map"
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
 * Name: mbv_spi_initialize
 *
 * Description:
 *   Initialize the AXI Quad SPI, register it as /dev/spi0 for the SPI tool
 *   and bind the configuration flash on slave select 0 to the M25P MTD
 *   driver as /dev/mtd0.  With LittleFS, mount the flash on /mnt/flash and
 *   format it if it does not contain a file system yet.
 *
 ****************************************************************************/

#ifdef CONFIG_MBV_SPI0
static void mbv_spi_initialize(void)
{
  struct spi_dev_s *spi;
#ifdef CONFIG_MTD_M25P
  struct mtd_dev_s *mtd;
#endif
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
  mtd = m25p_initialize(spi);
  if (mtd == NULL)
    {
      syslog(LOG_ERR, "ERROR: No M25P flash on AXI Quad SPI SS0\n");
      return;
    }

  ret = register_mtddriver("/dev/mtd0", mtd, 0755, NULL);
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
#endif /* CONFIG_MTD_M25P */
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
 *   register board specific drivers (e.g. AXI GPIO, AXI UART Lite).
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
