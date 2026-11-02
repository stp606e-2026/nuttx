/****************************************************************************
 * boards/risc-v/mbv/mbv-qemu/include/board.h
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

#ifndef __BOARDS_RISCV_MBV_MBV_QEMU_INCLUDE_BOARD_H
#define __BOARDS_RISCV_MBV_MBV_QEMU_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AXI INTC inputs of the QEMU amd-microblaze-v-generic machine.  These
 * are also the inputs used by the reference Vivado block design, see
 * Documentation/platforms/risc-v/mbv/boards/mbv-qemu/index.rst.
 */

#define MBV_INTC_IN_TIMER0      0  /* AXI Timer #1 (system tick) */
#define MBV_INTC_IN_UARTLITE    1  /* AXI UART Lite (edge) */
#define MBV_INTC_IN_QSPI        2  /* AXI Quad SPI (patched QEMU only) */
#define MBV_INTC_IN_UART16550   4  /* AXI UART16550 (console) */
#define MBV_INTC_IN_ETHLITE     5  /* AXI EthernetLite */
#define MBV_INTC_IN_TIMER1      6  /* AXI Timer #2 */
#define MBV_INTC_IN_AXIENET     7  /* AXI Ethernet */
#define MBV_INTC_IN_AXIDMA_S2MM 8  /* AXI DMA S2MM */
#define MBV_INTC_IN_AXIDMA_MM2S 9  /* AXI DMA MM2S */

/* NuttX IRQ numbers of the AXI INTC inputs above (MBV_IRQ_INTC() comes
 * from <arch/irq.h>)
 */

#define MBV_IRQ_UARTLITE        MBV_IRQ_INTC(MBV_INTC_IN_UARTLITE)
#define MBV_IRQ_UART16550       MBV_IRQ_INTC(MBV_INTC_IN_UART16550)
#define MBV_IRQ_ETHLITE         MBV_IRQ_INTC(MBV_INTC_IN_ETHLITE)
#define MBV_IRQ_TIMER1          MBV_IRQ_INTC(MBV_INTC_IN_TIMER1)

/* Peripheral base addresses not owned by the chip layer */

#define MBV_UARTLITE_BASE       0x40600000
#define MBV_UART16550_BASE      0x44a11000 /* 0x44a10000 + 0x1000 */
#define MBV_ETHLITE_BASE        0x40e00000
#define MBV_TIMER1_BASE         0x41c10000
#define MBV_QSPI_BASE           0x44a00000
#define MBV_GPIO0_BASE          0x40000000
#define MBV_GPIO1_BASE          0x40010000
#define MBV_GPIO2_BASE          0x40020000

#endif /* __BOARDS_RISCV_MBV_MBV_QEMU_INCLUDE_BOARD_H */
