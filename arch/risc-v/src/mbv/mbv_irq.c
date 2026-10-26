/****************************************************************************
 * arch/risc-v/src/mbv/mbv_irq.c
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

#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "riscv_internal.h"
#include "chip.h"
#include "hardware/mbv_axi_intc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Mask of the implemented AXI INTC inputs */

#if CONFIG_MBV_INTC_NIRQS >= 32
#  define MBV_INTC_ALL  0xffffffff
#else
#  define MBV_INTC_ALL  ((UINT32_C(1) << CONFIG_MBV_INTC_NIRQS) - 1)
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_intc_irq2mask
 *
 * Description:
 *   Return the AXI INTC input mask for a NuttX IRQ, or 0 if the IRQ is not
 *   an AXI INTC input.
 *
 ****************************************************************************/

static inline uint32_t mbv_intc_irq2mask(int irq)
{
  if (irq >= MBV_IRQ_INTC(0) && irq < NR_IRQS)
    {
      return UINT32_C(1) << (irq - MBV_IRQ_INTC(0));
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 ****************************************************************************/

void up_irqinitialize(void)
{
  /* Disable Machine interrupts */

  up_irq_save();

  /* Disable all AXI INTC inputs, then acknowledge anything that is still
   * latched in ISR from before the reset of the core.
   */

  putreg32(0, MBV_INTC_IER);
  putreg32(MBV_INTC_ALL, MBV_INTC_IAR);

  /* Enable the AXI INTC output (ME) and hardware interrupts (HIE).  Once
   * HIE is set, ISR can no longer be written by software and HIE cannot be
   * cleared again until the next reset.
   */

  putreg32(AXI_INTC_MER_ME | AXI_INTC_MER_HIE, MBV_INTC_MER);

  /* Attach the common interrupt handler */

  riscv_exception_attach();

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  /* Colorize the interrupt stack for debug purposes */

  riscv_color_intstack();

  /* And finally, enable interrupts */

  up_irq_enable();
#endif
}

/****************************************************************************
 * Name: up_disable_irq
 *
 * Description:
 *   Disable the IRQ specified by 'irq'.  AXI INTC inputs are masked in the
 *   AXI INTC, the core interrupts (software, timer) in mie.
 *
 ****************************************************************************/

void up_disable_irq(int irq)
{
  uint32_t mask = mbv_intc_irq2mask(irq);

  if (mask != 0)
    {
      /* AXI INTC input */

#ifdef CONFIG_MBV_INTC_HAS_SIE_CIE
      putreg32(mask, MBV_INTC_CIE);
#else
      modifyreg32(MBV_INTC_IER, mask, 0);
#endif
    }
  else if (irq == RISCV_IRQ_SOFT)
    {
      CLEAR_CSR(CSR_IE, IE_SIE);
    }
  else if (irq == RISCV_IRQ_TIMER)
    {
      CLEAR_CSR(CSR_IE, IE_TIE);
    }
}

/****************************************************************************
 * Name: up_enable_irq
 *
 * Description:
 *   Enable the IRQ specified by 'irq'.  AXI INTC inputs are unmasked in the
 *   AXI INTC, the core interrupts (software, timer) in mie.
 *
 ****************************************************************************/

void up_enable_irq(int irq)
{
  uint32_t mask = mbv_intc_irq2mask(irq);

  if (mask != 0)
    {
      /* AXI INTC input */

#ifdef CONFIG_MBV_INTC_HAS_SIE_CIE
      putreg32(mask, MBV_INTC_SIE);
#else
      modifyreg32(MBV_INTC_IER, 0, mask);
#endif
    }
  else if (irq == RISCV_IRQ_SOFT)
    {
      SET_CSR(CSR_IE, IE_SIE);
    }
  else if (irq == RISCV_IRQ_TIMER)
    {
      SET_CSR(CSR_IE, IE_TIE);
    }
}

/****************************************************************************
 * Name: up_irq_enable
 *
 * Description:
 *   Enable machine external interrupts and the global interrupt enable.
 *
 ****************************************************************************/

irqstate_t up_irq_enable(void)
{
  irqstate_t oldstat;

  /* Enable the machine external interrupt (MEIE).  This is the only path
   * from the AXI INTC to the core.
   */

  SET_CSR(CSR_IE, IE_EIE);

  /* Read and enable global interrupts (MIE) in mstatus */

  oldstat = READ_AND_SET_CSR(CSR_STATUS, STATUS_IE);

  return oldstat;
}
