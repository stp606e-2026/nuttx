/****************************************************************************
 * arch/risc-v/src/mbv/mbv_timerisr.c
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

#include <assert.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/mbv_axi_timer.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_SCHED_TICKLESS
#  error "Tickless mode is not supported yet"
#endif

#if CONFIG_MBV_TIMER_IRQ >= CONFIG_MBV_INTC_NIRQS
#  error "MBV_TIMER_IRQ is not a valid AXI INTC input"
#endif

#if (CONFIG_MBV_TIMER_CLK_FREQ % TICK_PER_SEC) != 0
#  warning "MBV_TIMER_CLK_FREQ is not a multiple of the tick rate"
#endif

#define MBV_IRQ_TIMER    MBV_IRQ_INTC(CONFIG_MBV_TIMER_IRQ)

/* Counter 0 counts down from TLR and reloads automatically.  The period of
 * a down counter is TLR + 2 timer clocks.
 */

#define MBV_TIMER_RELOAD AXI_TIMER_TLR_DOWN(CONFIG_MBV_TIMER_CLK_FREQ / \
                                            TICK_PER_SEC)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_timerisr
 *
 * Description:
 *   AXI Timer counter 0 interrupt.  The timer interrupt is level triggered:
 *   clear T0INT at the source first, the AXI INTC dispatcher acknowledges
 *   the input in IAR when this handler returns.
 *
 ****************************************************************************/

static int mbv_timerisr(int irq, void *context, void *arg)
{
  putreg32(getreg32(MBV_TIMER_TCSR0) | AXI_TIMER_TCSR_TINT,
           MBV_TIMER_TCSR0);

  nxsched_process_timer();
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize the system tick.
 *   AXI Timer counter 0 is used in generate mode, counting down with auto
 *   reload.  Counter 1 is left untouched (e.g. for a free running time
 *   base or tickless support later).
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  /* Stop counter 0 and clear a stale interrupt */

  putreg32(AXI_TIMER_TCSR_TINT, MBV_TIMER_TCSR0);

  /* Load the reload value into the counter */

  putreg32(MBV_TIMER_RELOAD, MBV_TIMER_TLR0);
  putreg32(AXI_TIMER_TCSR_LOAD, MBV_TIMER_TCSR0);

  /* Attach and enable the timer interrupt */

  irq_attach(MBV_IRQ_TIMER, mbv_timerisr, NULL);
  up_enable_irq(MBV_IRQ_TIMER);

  /* Clear LOAD and start counting down with auto reload */

  putreg32(AXI_TIMER_TCSR_UDT | AXI_TIMER_TCSR_ARHT |
           AXI_TIMER_TCSR_ENIT | AXI_TIMER_TCSR_ENT, MBV_TIMER_TCSR0);
}
