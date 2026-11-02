#!/usr/bin/env bash
############################################################################
# boards/risc-v/mbv/mbv-qemu/scripts/run-qemu.sh
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

# Run NuttX on the QEMU amd-microblaze-v-generic machine.
#
# Usage: run-qemu.sh [nuttx-elf] [nuttx-config]
#
#   nuttx-elf     defaults to ./nuttx
#   nuttx-config  defaults to the .config next to the ELF
#
# Environment:
#   QEMU   QEMU binary (default: qemu-system-riscv32)
#   EXTRA  extra QEMU arguments, e.g. EXTRA="-s -S" to wait for GDB on
#          localhost:1234
#   FLASH  16 MiB raw image backing the SPI flash (N25Q128A13) on the
#          AXI Quad SPI, created if missing.  The AXI Quad SPI is not part
#          of the upstream machine; it needs a QEMU built with
#          qemu-patches/ from the project repository.  Without FLASH
#          the flash contents are not saved.
#
# The machine ignores -kernel, so the ELF is loaded with the generic loader.
# With cpu-num set, the loader also sets the PC to the ELF entry point.
#
# Serial ports: serial_hd(0) is the AXI UART Lite, serial_hd(1) is the
# AXI UART16550 used as the NuttX console.

set -e

ELF=${1:-nuttx}
CONFIG=${2:-$(dirname "${ELF}")/.config}
QEMU=${QEMU:-qemu-system-riscv32}

if [ ! -f "${ELF}" ]; then
  echo "ERROR: ${ELF} not found" >&2
  exit 1
fi

# The default QEMU rv32 CPU implements many more extensions than a
# MicroBlaze V.  Start from the bare rv32i CPU (RV32I only) and enable just
# Zicsr, Zifencei and the extensions NuttX was built for, so that an
# instruction the real core lacks traps in QEMU as well.

isa_enabled()
{
  [ -f "${CONFIG}" ] && grep -q "^CONFIG_$1=y" "${CONFIG}"
}

onoff()
{
  if isa_enabled "$1"; then echo true; else echo false; fi
}

if [ ! -f "${CONFIG}" ]; then
  echo "WARNING: ${CONFIG} not found, assuming rv32imac" >&2
  M=true; A=true; C=true; F=false
else
  M=$(onoff ARCH_RV_ISA_M)
  A=$(onoff ARCH_RV_ISA_A)
  C=$(onoff ARCH_RV_ISA_C)
  F=$(onoff ARCH_FPU)
fi

DRIVE=
if [ -n "${FLASH}" ]; then
  if [ ! -f "${FLASH}" ]; then
    dd if=/dev/zero bs=1048576 count=16 2>/dev/null | \
      LC_ALL=C tr '\000' '\377' > "${FLASH}"
  fi
  DRIVE="-drive if=mtd,format=raw,file=${FLASH}"
fi

CPU="rv32i,zicsr=true,zifencei=true,m=${M},a=${A},c=${C},f=${F}"

echo "QEMU CPU: ${CPU}" >&2

exec ${QEMU} -M amd-microblaze-v-generic -m 256M \
  -cpu "${CPU}" -display none \
  -serial null -serial mon:stdio \
  -device loader,file="${ELF}",cpu-num=0 ${DRIVE} \
  -d unimp,guest_errors ${EXTRA}
