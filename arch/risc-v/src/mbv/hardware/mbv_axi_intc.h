/****************************************************************************
 * arch/risc-v/src/mbv/hardware/mbv_axi_intc.h
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

#ifndef __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_INTC_H
#define __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_INTC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hardware/mbv_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AXI Interrupt Controller register offsets (AMD PG099) */

#define AXI_INTC_ISR_OFFSET   0x00  /* Interrupt Status Register */
#define AXI_INTC_IPR_OFFSET   0x04  /* Interrupt Pending Register */
#define AXI_INTC_IER_OFFSET   0x08  /* Interrupt Enable Register */
#define AXI_INTC_IAR_OFFSET   0x0c  /* Interrupt Acknowledge Register */
#define AXI_INTC_SIE_OFFSET   0x10  /* Set Interrupt Enables */
#define AXI_INTC_CIE_OFFSET   0x14  /* Clear Interrupt Enables */
#define AXI_INTC_IVR_OFFSET   0x18  /* Interrupt Vector Register */
#define AXI_INTC_MER_OFFSET   0x1c  /* Master Enable Register */
#define AXI_INTC_IMR_OFFSET   0x20  /* Interrupt Mode Register */
#define AXI_INTC_ILR_OFFSET   0x24  /* Interrupt Level Register */

/* Interrupt Vector Address Registers (C_HAS_FAST), one per input */

#define AXI_INTC_IVAR_OFFSET(n) (0x100 + 4 * (n))

/* AXI Interrupt Controller register addresses */

#define MBV_INTC_ISR          (MBV_INTC_BASE + AXI_INTC_ISR_OFFSET)
#define MBV_INTC_IPR          (MBV_INTC_BASE + AXI_INTC_IPR_OFFSET)
#define MBV_INTC_IER          (MBV_INTC_BASE + AXI_INTC_IER_OFFSET)
#define MBV_INTC_IAR          (MBV_INTC_BASE + AXI_INTC_IAR_OFFSET)
#define MBV_INTC_SIE          (MBV_INTC_BASE + AXI_INTC_SIE_OFFSET)
#define MBV_INTC_CIE          (MBV_INTC_BASE + AXI_INTC_CIE_OFFSET)
#define MBV_INTC_IVR          (MBV_INTC_BASE + AXI_INTC_IVR_OFFSET)
#define MBV_INTC_MER          (MBV_INTC_BASE + AXI_INTC_MER_OFFSET)
#define MBV_INTC_IMR          (MBV_INTC_BASE + AXI_INTC_IMR_OFFSET)
#define MBV_INTC_ILR          (MBV_INTC_BASE + AXI_INTC_ILR_OFFSET)
#define MBV_INTC_IVAR(n)      (MBV_INTC_BASE + AXI_INTC_IVAR_OFFSET(n))

/* Master Enable Register (MER) */

#define AXI_INTC_MER_ME       (1 << 0)  /* Master IRQ Enable */
#define AXI_INTC_MER_HIE      (1 << 1)  /* Hardware Interrupt Enable */

/* Interrupt Vector Register (IVR): all ones when nothing is pending */

#define AXI_INTC_IVR_NONE     0xffffffff

#endif /* __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_INTC_H */
