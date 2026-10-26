/****************************************************************************
 * arch/risc-v/src/mbv/mbv_head.S
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
#include <nuttx/irq.h>

#include <arch/arch.h>
#include <arch/irq.h>

#include "chip.h"

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

  /* Exported Symbols */

  .section .text
  .global __start
  .type   __start, function

__start:

  /* The core starts in M-mode.  Make sure that all interrupts are
   * disabled until up_irqinitialize() has set up the AXI INTC.
   */

  csrw  CSR_MIE, zero
  csrci CSR_MSTATUS, MSTATUS_MIE

  /* Only hart 0 runs NuttX, park any other hart */

  csrr  a0, CSR_MHARTID
  bnez  a0, 1f

  /* Install the common trap vector in direct mode.  __trap_vec is 64 byte
   * aligned, so mtvec.MODE (bits 1:0) is 0 (direct).
   */

  la    t0, __trap_vec
  csrw  CSR_MTVEC, t0

  /* Set the stack pointer to the top of the idle thread stack that starts
   * right after .bss (g_idle_topstack).  This does not use
   * riscv_set_inital_sp because it needs the M extension.
   */

  la    sp, _ebss
  li    t0, STACKFRAME_ALIGN_DOWN(SMP_STACK_SIZE)
  add   sp, sp, t0

  /* Jump to mbv_start, it never returns */

  jal   x1, mbv_start

1:
  wfi
  j     1b

  .size __start, . - __start

  /* These don't have to do anything since we use init_array/fini_array. */

  .global _init
  .global _fini

_init:
_fini:
  ret
