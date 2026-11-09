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

/* Memory map of the soc_bd design, bitstream v0.2.  The image is linked to
 * CONFIG_RAM_START: the 256 KiB LMB BRAM to be embedded in the bitstream or
 * the DDR to be loaded over JTAG.
 */

#define MBV_BRAM_BASE           0x00000000
#define MBV_BRAM_SIZE           (256 * 1024)
#define MBV_DDR_BASE            0x80000000
#define MBV_DDR_SIZE            (256 * 1024 * 1024)

/* AXI INTC inputs, in the order of the xlconcat of the block design */

#define MBV_INTC_NINPUTS        3
#define MBV_INTC_IN_TIMER0      0  /* AXI Timer (system tick) */
#define MBV_INTC_IN_UARTLITE    1  /* AXI UART Lite */
#define MBV_INTC_IN_QSPI        2  /* AXI Quad SPI (SD card) */

/* Edge-triggered AXI INTC inputs (C_KIND_OF_INTR = 0xfffffffe in the
 * hardware handoff): the UART Lite and Quad SPI interrupts are rising
 * edges, the AXI Timer interrupt is level high.
 */

#define MBV_INTC_EDGE_INPUTS    ((1 << MBV_INTC_IN_UARTLITE) | \
                                 (1 << MBV_INTC_IN_QSPI))

/* NuttX IRQ numbers of the AXI INTC inputs above (MBV_IRQ_INTC() comes
 * from <arch/irq.h>)
 */

#define MBV_IRQ_UARTLITE        MBV_IRQ_INTC(MBV_INTC_IN_UARTLITE)
#define MBV_IRQ_QSPI            MBV_IRQ_INTC(MBV_INTC_IN_QSPI)

/* Peripheral base addresses not owned by the chip layer */

#define MBV_UARTLITE_BASE       0x40600000
#define MBV_QSPI_BASE           0x44a00000

/* AXI Quad SPI: the SD card (HW-125 module on Pmod JA) is on slave select
 * 0.  SCK is fixed in hardware to ext_spi_clk (5 MHz) / C_SCK_RATIO (16).
 */

#define MBV_QSPI_FIFO_DEPTH     256
#define MBV_QSPI_SCK_FREQUENCY  312500

#endif /* __BOARDS_RISCV_MBV_MBV_ARTY_INCLUDE_BOARD_H */
