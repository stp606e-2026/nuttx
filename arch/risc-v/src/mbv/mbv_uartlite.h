/****************************************************************************
 * arch/risc-v/src/mbv/mbv_uartlite.h
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

#ifndef __ARCH_RISCV_SRC_MBV_MBV_UARTLITE_H
#define __ARCH_RISCV_SRC_MBV_MBV_UARTLITE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#ifdef CONFIG_MBV_UARTLITE0

/****************************************************************************
 * Name: mbv_uartlite_earlyserialinit
 *
 * Description:
 *   Configure the AXI UART Lite before the serial driver is registered so
 *   that it can be used as the console early in the boot sequence.
 *   Called from riscv_earlyserialinit().
 *
 ****************************************************************************/

void mbv_uartlite_earlyserialinit(void);

/****************************************************************************
 * Name: mbv_uartlite_serialinit
 *
 * Description:
 *   Register the AXI UART Lite with the serial driver.  Called from
 *   riscv_serialinit().
 *
 ****************************************************************************/

void mbv_uartlite_serialinit(void);

#endif /* CONFIG_MBV_UARTLITE0 */

#if defined(__cplusplus)
}
#endif
#undef EXTERN

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_RISCV_SRC_MBV_MBV_UARTLITE_H */
