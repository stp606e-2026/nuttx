/****************************************************************************
 * arch/risc-v/src/mbv/hardware/mbv_axi_uartlite.h
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

#ifndef __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_UARTLITE_H
#define __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_UARTLITE_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AXI UART Lite register offsets (AMD PG142).  All registers are 32 bits
 * wide, the data registers use bits 7:0 only.
 */

#define AXI_UARTLITE_RX_OFFSET      0x00 /* RX FIFO (read only) */
#define AXI_UARTLITE_TX_OFFSET      0x04 /* TX FIFO (write only) */
#define AXI_UARTLITE_STAT_OFFSET    0x08 /* Status (read only) */
#define AXI_UARTLITE_CTRL_OFFSET    0x0c /* Control (write only) */

/* Status Register (STAT).  The error bits are cleared when STAT is read. */

#define AXI_UARTLITE_STAT_RXVALID   (1 << 0) /* RX FIFO has data */
#define AXI_UARTLITE_STAT_RXFULL    (1 << 1) /* RX FIFO full */
#define AXI_UARTLITE_STAT_TXEMPTY   (1 << 2) /* TX FIFO empty */
#define AXI_UARTLITE_STAT_TXFULL    (1 << 3) /* TX FIFO full */
#define AXI_UARTLITE_STAT_INTREN    (1 << 4) /* Interrupt enabled */
#define AXI_UARTLITE_STAT_OVERRUN   (1 << 5) /* RX overrun error */
#define AXI_UARTLITE_STAT_FRAME     (1 << 6) /* RX frame error */
#define AXI_UARTLITE_STAT_PARITY    (1 << 7) /* RX parity error */

/* Control Register (CTRL) */

#define AXI_UARTLITE_CTRL_RSTTX     (1 << 0) /* Reset TX FIFO */
#define AXI_UARTLITE_CTRL_RSTRX     (1 << 1) /* Reset RX FIFO */
#define AXI_UARTLITE_CTRL_INTREN    (1 << 4) /* Interrupt enable */

/* Depth of the RX and TX FIFOs */

#define AXI_UARTLITE_FIFO_DEPTH     16

#endif /* __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_UARTLITE_H */
