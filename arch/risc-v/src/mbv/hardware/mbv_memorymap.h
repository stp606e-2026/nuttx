/****************************************************************************
 * arch/risc-v/src/mbv/hardware/mbv_memorymap.h
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

#ifndef __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_MEMORYMAP_H
#define __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_MEMORYMAP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The MicroBlaze V is a soft core: the peripheral addresses depend on the
 * Vivado block design.  They are configured through Kconfig and default to
 * the Vivado address editor defaults, which are also what the QEMU
 * amd-microblaze-v-generic machine uses.
 */

#define MBV_INTC_BASE   CONFIG_MBV_INTC_BASE
#define MBV_TIMER_BASE  CONFIG_MBV_TIMER_BASE

#endif /* __ARCH_RISCV_SRC_MBV_HARDWARE_MBV_MEMORYMAP_H */
