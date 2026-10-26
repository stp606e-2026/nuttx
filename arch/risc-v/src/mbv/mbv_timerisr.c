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

/* Timer clocks per system tick */

#define MBV_TICK_CYCLES  ((uint32_t)(CONFIG_MBV_TIMER_CLK_FREQ / TICK_PER_SEC))

/* Counter 0 counts down from TLR and reloads automatically.  The period of
 * a down counter is TLR + 2 timer clocks.
 */

#define MBV_TIMER_RELOAD AXI_TIMER_TLR_DOWN(MBV_TICK_CYCLES)

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_MBV_TIMER_TICK_CATCHUP
/* Counter 1 value at the end of the last processed tick */

static uint32_t g_mbv_tick_last;
#endif

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
 *   With MBV_TIMER_TICK_CATCHUP, one tick is processed for every tick
 *   period elapsed on the free running counter 1, so a late interrupt
 *   processes the missed ticks as well.  The unsigned subtraction handles
 *   the counter 1 wrap around.
 *
 ****************************************************************************/

static int mbv_timerisr(int irq, void *context, void *arg)
{
#ifdef CONFIG_MBV_TIMER_TICK_CATCHUP
  uint32_t now;
#endif

  putreg32(getreg32(MBV_TIMER_TCSR0) | AXI_TIMER_TCSR_TINT,
           MBV_TIMER_TCSR0);

#ifdef CONFIG_MBV_TIMER_TICK_CATCHUP
  now = getreg32(MBV_TIMER_TCR1);

  while (now - g_mbv_tick_last >= MBV_TICK_CYCLES)
    {
      g_mbv_tick_last += MBV_TICK_CYCLES;
      nxsched_process_timer();
    }
#else
  nxsched_process_timer();
#endif

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
 *   reload, to raise the tick interrupt.  With MBV_TIMER_TICK_CATCHUP,
 *   counter 1 runs as a free running up counter without interrupt and is
 *   the time reference.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  /* Stop the counters and clear stale interrupts */

  putreg32(AXI_TIMER_TCSR_TINT, MBV_TIMER_TCSR0);
#ifdef CONFIG_MBV_TIMER_TICK_CATCHUP
  putreg32(AXI_TIMER_TCSR_TINT, MBV_TIMER_TCSR1);

  /* Start counter 1 from 0, counting up with auto reload (wrap around) */

  putreg32(0, MBV_TIMER_TLR1);
  putreg32(AXI_TIMER_TCSR_LOAD, MBV_TIMER_TCSR1);
  putreg32(AXI_TIMER_TCSR_ARHT | AXI_TIMER_TCSR_ENT, MBV_TIMER_TCSR1);
#endif

  /* Load the reload value into the counter */

  putreg32(MBV_TIMER_RELOAD, MBV_TIMER_TLR0);
  putreg32(AXI_TIMER_TCSR_LOAD, MBV_TIMER_TCSR0);

  /* Attach and enable the timer interrupt */

  irq_attach(MBV_IRQ_TIMER, mbv_timerisr, NULL);
  up_enable_irq(MBV_IRQ_TIMER);

  /* Take the reference before counter 0 starts, so that the first tick
   * boundary on counter 1 is never after the first counter 0 interrupt.
   */

#ifdef CONFIG_MBV_TIMER_TICK_CATCHUP
  g_mbv_tick_last = getreg32(MBV_TIMER_TCR1);
#endif

  /* Clear LOAD and start counting down with auto reload */

  putreg32(AXI_TIMER_TCSR_UDT | AXI_TIMER_TCSR_ARHT |
           AXI_TIMER_TCSR_ENIT | AXI_TIMER_TCSR_ENT, MBV_TIMER_TCSR0);
}
