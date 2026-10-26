/****************************************************************************
 * arch/risc-v/src/mbv/mbv_irq_dispatch.c
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

#include <stdint.h>
#include <assert.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/mbv_axi_intc.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_intc_pending
 *
 * Description:
 *   Return the lowest numbered pending and enabled AXI INTC input, or -1 if
 *   there is none.
 *
 ****************************************************************************/

static inline int mbv_intc_pending(void)
{
#ifdef CONFIG_MBV_INTC_HAS_IVR
  uint32_t ivr = getreg32(MBV_INTC_IVR);

  return ivr == AXI_INTC_IVR_NONE ? -1 : (int)ivr;
#else
  uint32_t ipr = getreg32(MBV_INTC_IPR);

  return ipr == 0 ? -1 : __builtin_ctz(ipr);
#endif
}

/****************************************************************************
 * Name: mbv_dispatch_intc
 *
 * Description:
 *   Service the AXI INTC until no enabled input is pending.
 *
 *   Level-triggered inputs are acknowledged after the handler: the handler
 *   must first clear the request at the source, otherwise the INTC would
 *   latch it again right away.  Edge-triggered inputs are acknowledged
 *   before the handler, so that an edge arriving while the handler runs is
 *   kept pending instead of being cleared by a late IAR write.
 *
 ****************************************************************************/

static uintreg_t *mbv_dispatch_intc(uintreg_t *regs)
{
  uint32_t mask;
  int extirq;

  while ((extirq = mbv_intc_pending()) >= 0)
    {
      DEBUGASSERT(extirq < CONFIG_MBV_INTC_NIRQS);

      mask = UINT32_C(1) << extirq;

      if ((CONFIG_MBV_INTC_EDGE_MASK & mask) != 0)
        {
          putreg32(mask, MBV_INTC_IAR);
          regs = riscv_doirq(MBV_IRQ_INTC(extirq), regs);
        }
      else
        {
          regs = riscv_doirq(MBV_IRQ_INTC(extirq), regs);
          putreg32(mask, MBV_INTC_IAR);
        }
    }

  return regs;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: riscv_dispatch_irq
 *
 * Description:
 *   Called from exception_common with the mcause value.  The machine
 *   external interrupt is the AXI INTC output and is demultiplexed here,
 *   everything else goes straight to riscv_doirq().
 *
 ****************************************************************************/

void *riscv_dispatch_irq(uintreg_t vector, uintreg_t *regs)
{
  int irq = vector & RISCV_IRQ_MASK;

  if ((vector & RISCV_IRQ_BIT) != 0)
    {
      irq += RISCV_IRQ_ASYNC;

      if (irq == RISCV_IRQ_EXT)
        {
          return mbv_dispatch_intc(regs);
        }
    }

  return riscv_doirq(irq, regs);
}
