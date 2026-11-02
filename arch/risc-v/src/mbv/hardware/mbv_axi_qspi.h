/****************************************************************************
 * arch/risc-v/src/mbv/hardware/mbv_axi_qspi.h
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

#ifndef __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_QSPI_H
#define __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_QSPI_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AXI Quad SPI register offsets (AMD PG153), legacy (non-XIP) mode.  The
 * same register set is used in standard, dual and quad SPI mode.
 */

#define AXI_QSPI_DGIER_OFFSET     0x1c /* Device Global Interrupt Enable */
#define AXI_QSPI_IPISR_OFFSET     0x20 /* IP Interrupt Status */
#define AXI_QSPI_IPIER_OFFSET     0x28 /* IP Interrupt Enable */
#define AXI_QSPI_SRR_OFFSET       0x40 /* Software Reset */
#define AXI_QSPI_SPICR_OFFSET     0x60 /* SPI Control */
#define AXI_QSPI_SPISR_OFFSET     0x64 /* SPI Status */
#define AXI_QSPI_DTR_OFFSET       0x68 /* SPI Data Transmit (TX FIFO) */
#define AXI_QSPI_DRR_OFFSET       0x6c /* SPI Data Receive (RX FIFO) */
#define AXI_QSPI_SSR_OFFSET       0x70 /* SPI Slave Select */
#define AXI_QSPI_TXOCY_OFFSET     0x74 /* TX FIFO occupancy - 1 */
#define AXI_QSPI_RXOCY_OFFSET     0x78 /* RX FIFO occupancy - 1 */

/* Software Reset Register (SRR) */

#define AXI_QSPI_SRR_RESET        0x0000000a /* Only this value resets */

/* Device Global Interrupt Enable Register (DGIER) */

#define AXI_QSPI_DGIER_GIE        (1u << 31)

/* SPI Control Register (SPICR) */

#define AXI_QSPI_SPICR_LOOP       (1 << 0)  /* Local loopback */
#define AXI_QSPI_SPICR_SPE        (1 << 1)  /* SPI system enable */
#define AXI_QSPI_SPICR_MASTER     (1 << 2)  /* Master mode */
#define AXI_QSPI_SPICR_CPOL       (1 << 3)  /* Clock polarity */
#define AXI_QSPI_SPICR_CPHA       (1 << 4)  /* Clock phase */
#define AXI_QSPI_SPICR_TXRST      (1 << 5)  /* TX FIFO reset (self clearing) */
#define AXI_QSPI_SPICR_RXRST      (1 << 6)  /* RX FIFO reset (self clearing) */
#define AXI_QSPI_SPICR_MSSAE      (1 << 7)  /* Manual slave select enable */
#define AXI_QSPI_SPICR_MTI        (1 << 8)  /* Master transaction inhibit */
#define AXI_QSPI_SPICR_LSBF       (1 << 9)  /* LSB first */

/* SPI Status Register (SPISR) */

#define AXI_QSPI_SPISR_RXEMPTY    (1 << 0)  /* RX FIFO empty */
#define AXI_QSPI_SPISR_RXFULL     (1 << 1)  /* RX FIFO full */
#define AXI_QSPI_SPISR_TXEMPTY    (1 << 2)  /* TX FIFO empty */
#define AXI_QSPI_SPISR_TXFULL     (1 << 3)  /* TX FIFO full */
#define AXI_QSPI_SPISR_MODF       (1 << 4)  /* Mode fault */
#define AXI_QSPI_SPISR_SLVMS      (1 << 5)  /* Slave mode select */
#define AXI_QSPI_SPISR_CPOLCPHA   (1 << 6)  /* CPOL/CPHA error */
#define AXI_QSPI_SPISR_SLVMODE    (1 << 7)  /* Slave mode error */
#define AXI_QSPI_SPISR_MSBERR     (1 << 8)  /* MSB error */
#define AXI_QSPI_SPISR_LOOPERR    (1 << 9)  /* Loopback error */
#define AXI_QSPI_SPISR_CMDERR     (1 << 10) /* Command error */

/* IP Interrupt Status/Enable Registers (IPISR/IPIER) */

#define AXI_QSPI_INT_MODF         (1 << 0)  /* Mode fault */
#define AXI_QSPI_INT_SLVMODF      (1 << 1)  /* Slave mode fault */
#define AXI_QSPI_INT_DTREMPTY     (1 << 2)  /* DTR (TX FIFO) empty */
#define AXI_QSPI_INT_DTRUNDER     (1 << 3)  /* DTR underrun */
#define AXI_QSPI_INT_DRRFULL      (1 << 4)  /* DRR (RX FIFO) full */
#define AXI_QSPI_INT_DRROVER      (1 << 5)  /* DRR overrun */
#define AXI_QSPI_INT_TXHALF       (1 << 6)  /* TX FIFO half empty */
#define AXI_QSPI_INT_SLVSEL       (1 << 7)  /* Slave select mode */
#define AXI_QSPI_INT_DRRNEMPTY    (1 << 8)  /* DRR not empty */

#endif /* __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_QSPI_H */
