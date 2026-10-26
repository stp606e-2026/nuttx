/****************************************************************************
 * arch/risc-v/src/mbv/hardware/mbv_axi_timer.h
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

#ifndef __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_TIMER_H
#define __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_TIMER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "hardware/mbv_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AXI Timer register offsets (AMD PG079).  The two counters of one AXI
 * Timer instance use the same layout, counter 1 is at +0x10.
 */

#define AXI_TIMER_TCSR_OFFSET(n)  (0x10 * (n) + 0x00) /* Control/Status */
#define AXI_TIMER_TLR_OFFSET(n)   (0x10 * (n) + 0x04) /* Load */
#define AXI_TIMER_TCR_OFFSET(n)   (0x10 * (n) + 0x08) /* Counter */

#define AXI_TIMER_TCSR0_OFFSET    AXI_TIMER_TCSR_OFFSET(0)
#define AXI_TIMER_TLR0_OFFSET     AXI_TIMER_TLR_OFFSET(0)
#define AXI_TIMER_TCR0_OFFSET     AXI_TIMER_TCR_OFFSET(0)
#define AXI_TIMER_TCSR1_OFFSET    AXI_TIMER_TCSR_OFFSET(1)
#define AXI_TIMER_TLR1_OFFSET     AXI_TIMER_TLR_OFFSET(1)
#define AXI_TIMER_TCR1_OFFSET     AXI_TIMER_TCR_OFFSET(1)

/* System tick AXI Timer register addresses */

#define MBV_TIMER_TCSR0           (MBV_TIMER_BASE + AXI_TIMER_TCSR0_OFFSET)
#define MBV_TIMER_TLR0            (MBV_TIMER_BASE + AXI_TIMER_TLR0_OFFSET)
#define MBV_TIMER_TCR0            (MBV_TIMER_BASE + AXI_TIMER_TCR0_OFFSET)
#define MBV_TIMER_TCSR1           (MBV_TIMER_BASE + AXI_TIMER_TCSR1_OFFSET)
#define MBV_TIMER_TLR1            (MBV_TIMER_BASE + AXI_TIMER_TLR1_OFFSET)
#define MBV_TIMER_TCR1            (MBV_TIMER_BASE + AXI_TIMER_TCR1_OFFSET)

/* Timer Control/Status Register (TCSR0/TCSR1) */

#define AXI_TIMER_TCSR_MDT        (1 << 0)  /* Mode: 1 = capture */
#define AXI_TIMER_TCSR_UDT        (1 << 1)  /* 1 = count down */
#define AXI_TIMER_TCSR_GENT       (1 << 2)  /* Enable external generate */
#define AXI_TIMER_TCSR_CAPT       (1 << 3)  /* Enable external capture */
#define AXI_TIMER_TCSR_ARHT       (1 << 4)  /* Auto reload / hold */
#define AXI_TIMER_TCSR_LOAD       (1 << 5)  /* Load TLR into the counter */
#define AXI_TIMER_TCSR_ENIT       (1 << 6)  /* Enable interrupt */
#define AXI_TIMER_TCSR_ENT        (1 << 7)  /* Enable timer */
#define AXI_TIMER_TCSR_TINT       (1 << 8)  /* Interrupt, write 1 to clear */
#define AXI_TIMER_TCSR_PWMA       (1 << 9)  /* Enable PWM */
#define AXI_TIMER_TCSR_ENALL      (1 << 10) /* Enable all timers */
#define AXI_TIMER_TCSR_CASC       (1 << 11) /* Cascade mode (TCSR0 only) */

/* In down-counting generate mode the period is TLR + 2 timer clocks */

#define AXI_TIMER_TLR_DOWN(cycles) ((cycles) - 2)

#endif /* __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_AXI_TIMER_H */
