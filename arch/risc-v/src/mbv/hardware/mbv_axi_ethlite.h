/****************************************************************************
 * arch/risc-v/src/mbv/hardware/mbv_axi_ethlite.h
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

#ifndef __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_ETHLITE_H
#define __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_ETHLITE_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AXI EthernetLite register offsets (AMD PG135).  The core has one TX and
 * one RX packet buffer (ping), plus a second one of each (pong) if it was
 * generated with C_TX_PING_PONG / C_RX_PING_PONG.  Each pong buffer and its
 * registers are at AXI_ETHLITE_PONG_OFFSET from the ping ones.
 */

#define AXI_ETHLITE_TXBUF_OFFSET    0x0000 /* TX ping buffer */
#define AXI_ETHLITE_MDIOADDR_OFFSET 0x07e4 /* MDIO address */
#define AXI_ETHLITE_MDIOWR_OFFSET   0x07e8 /* MDIO write data */
#define AXI_ETHLITE_MDIORD_OFFSET   0x07ec /* MDIO read data */
#define AXI_ETHLITE_MDIOCTRL_OFFSET 0x07f0 /* MDIO control */
#define AXI_ETHLITE_TXLEN_OFFSET    0x07f4 /* TX ping length */
#define AXI_ETHLITE_GIE_OFFSET      0x07f8 /* Global interrupt enable */
#define AXI_ETHLITE_TXCTRL_OFFSET   0x07fc /* TX ping control */
#define AXI_ETHLITE_RXBUF_OFFSET    0x1000 /* RX ping buffer */
#define AXI_ETHLITE_RXCTRL_OFFSET   0x17fc /* RX ping control */

#define AXI_ETHLITE_PONG_OFFSET     0x0800 /* Pong buffer/registers */

/* Size of each packet buffer, up to the first register behind it */

#define AXI_ETHLITE_BUFSIZE         AXI_ETHLITE_MDIOADDR_OFFSET

/* Global Interrupt Enable Register (GIE) */

#define AXI_ETHLITE_GIE_GIE         (1u << 31)

/* TX Control Register (TXCTRL) */

#define AXI_ETHLITE_TXCTRL_STATUS   (1 << 0)  /* Busy, set to start */
#define AXI_ETHLITE_TXCTRL_PROGRAM  (1 << 1)  /* With STATUS: load the MAC */
#define AXI_ETHLITE_TXCTRL_IE       (1 << 3)  /* TX done interrupt enable */
#define AXI_ETHLITE_TXCTRL_LOOPBACK (1 << 4)  /* Internal loopback */

/* RX Control Register (RXCTRL) */

#define AXI_ETHLITE_RXCTRL_STATUS   (1 << 0)  /* Packet ready, write 0 */
#define AXI_ETHLITE_RXCTRL_IE       (1 << 3)  /* RX interrupt enable */

#endif /* __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_ETHLITE_H */
