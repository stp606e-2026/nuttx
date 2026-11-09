/****************************************************************************
 * boards/risc-v/mbv/mbv-arty/include/board.h
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

#ifndef __BOARDS_RISCV_MBV_MBV_ARTY_INCLUDE_BOARD_H
#define __BOARDS_RISCV_MBV_MBV_ARTY_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Memory map of the soc_bd design (Vivado Address Editor).  The image is
 * linked to CONFIG_RAM_START: the LMB BRAM to be embedded in the bitstream
 * or the DDR to be loaded over JTAG.
 */

#define MBV_BRAM_BASE           0x00000000
#define MBV_DDR_BASE            0x80000000
#define MBV_DDR_SIZE            (256 * 1024 * 1024)

/* AXI INTC inputs (xlconcat order).  Only the timer input is verified
 * against the design so far, the others match the QEMU arty-a7-mbv
 * machine.
 */

#define MBV_INTC_IN_TIMER0      0  /* AXI Timer (system tick) */
#define MBV_INTC_IN_UARTLITE    1  /* AXI UART Lite (edge), unverified */
#define MBV_INTC_IN_QSPI        2  /* AXI Quad SPI, unverified */

/* NuttX IRQ numbers of the AXI INTC inputs above (MBV_IRQ_INTC() comes
 * from <arch/irq.h>)
 */

#define MBV_IRQ_UARTLITE        MBV_IRQ_INTC(MBV_INTC_IN_UARTLITE)
#define MBV_IRQ_QSPI            MBV_IRQ_INTC(MBV_INTC_IN_QSPI)

/* Peripheral base addresses not owned by the chip layer */

#define MBV_UARTLITE_BASE       0x40600000
#define MBV_QSPI_BASE           0x44a00000

#endif /* __BOARDS_RISCV_MBV_MBV_ARTY_INCLUDE_BOARD_H */
