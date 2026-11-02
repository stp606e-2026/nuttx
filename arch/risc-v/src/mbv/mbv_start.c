/****************************************************************************
 * arch/risc-v/src/mbv/mbv_start.c
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

#include <nuttx/init.h>
#include <nuttx/arch.h>
#include <nuttx/serial/uart_16550.h>
#include <arch/board/board.h>

#include "riscv_internal.h"
#include "chip.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if !defined(CONFIG_BUILD_FLAT) || defined(CONFIG_SMP) || \
    defined(CONFIG_ARCH_USE_S_MODE)
#  error "MicroBlaze V only supports the flat, single hart, M-mode build"
#endif

#ifdef CONFIG_DEBUG_FEATURES
#  define showprogress(c) up_putc(c)
#else
#  define showprogress(c)
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_clear_bss
 ****************************************************************************/

static void mbv_clear_bss(void)
{
  uint32_t *dest;

  /* Clear .bss.  We'll do this inline (vs. calling memset) just to be
   * certain that there are no issues with the state of global variables.
   */

  for (dest = (uint32_t *)_sbss; dest < (uint32_t *)_ebss; )
    {
      *dest++ = 0;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mbv_start
 *
 * Description:
 *   C entry point, called from __start in mbv_head.S with interrupts
 *   disabled and the stack pointer set to the idle thread stack.
 *
 *   The image is loaded into RAM by the debugger or by the QEMU generic
 *   loader, so .data is already in place and only .bss needs clearing.
 *
 ****************************************************************************/

void mbv_start(void)
{
  /* Configure the FPU (no-op unless CONFIG_ARCH_FPU) */

  riscv_fpuconfig();

  mbv_clear_bss();

  /* The 16550 up_putc() works by polling, so this is visible even before
   * the serial driver and the interrupt controller are initialized.
   */

  showprogress('A');

#ifdef USE_EARLYSERIALINIT
  riscv_earlyserialinit();
#endif

  showprogress('B');

  /* Call nx_start() */

  nx_start();

  /* Shouldn't get here */

  for (; ; )
    {
      asm("WFI");
    }
}

/****************************************************************************
 * Name: riscv_earlyserialinit
 ****************************************************************************/

void riscv_earlyserialinit(void)
{
#ifdef CONFIG_16550_UART
  u16550_earlyserialinit();
#endif
}

/****************************************************************************
 * Name: riscv_serialinit
 ****************************************************************************/

void riscv_serialinit(void)
{
#ifdef CONFIG_16550_UART
  u16550_serialinit();
#endif
}

/****************************************************************************
 * Name: riscv_netinitialize
 *
 * Description:
 *   Networking without a MAC driver (e.g. loopback only).  The AXI
 *   EthernetLite driver provides its own riscv_netinitialize().
 *
 ****************************************************************************/

#if defined(CONFIG_NET) && !defined(CONFIG_NETDEV_LATEINIT) && \
    !defined(CONFIG_MBV_ETHLITE)
void riscv_netinitialize(void)
{
}
#endif
