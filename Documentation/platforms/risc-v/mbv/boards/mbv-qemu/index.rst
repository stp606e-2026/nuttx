========
mbv-qemu
========

.. tags:: arch:risc-v, chip:mbv, vendor:amd

MicroBlaze V on the QEMU ``amd-microblaze-v-generic`` machine.  The machine
models a MicroBlaze V with the AXI IPs and address map of a default Vivado
block design, so this board is also the starting point for a real
MicroBlaze V design such as the Digilent Arty A7.

Memory Map and Interrupts
=========================

=====================  ============  =========  ==============================
IP                     Base          INTC in    NuttX
=====================  ============  =========  ==============================
LMB BRAM (128 KiB)     0x00000000    \-         Not used
DDR (``-m``)           0x80000000    \-         Code, data, heap
AXI INTC               0x41200000    \-         Output to MEIP
AXI Timer #1           0x41c00000    0          System tick (counters 0, 1)
AXI UART Lite          0x40600000    1 (edge)   Not supported
AXI UART16550          0x44a11000    4          Console
AXI EthernetLite       0x40e00000    5          Not supported
AXI Timer #2           0x41c10000    6          Not used
AXI Ethernet           0x40c00000    7          Not supported
AXI DMA                0x41e00000    9 / 8      Not supported (MM2S / S2MM)
AXI GPIO x3            0x40000000    \-         Not implemented by QEMU
                       0x40010000
                       0x40020000
AXI IIC                0x40800000    \-         Not implemented by QEMU
AXI Quad SPI           0x44a00000    \-         Not implemented by QEMU
=====================  ============  =========  ==============================

The AXI UART16550 registers start at offset 0x1000 of the IP (base
0x44a10000) and are 32 bits wide on a 4 byte stride.  With the generic
16550 driver this is ``CONFIG_16550_REGINCR=1`` (the stride is
``REGINCR * REGWIDTH / 8``) and ``CONFIG_16550_REGWIDTH=32``.  Its IRQ is
``RISCV_IRQ_EXT + 4`` = 31; the board checks this at build time.

Configurations
==============

nsh
---

Flat build with the AXI UART16550 console, ``ostest``, procfs and the IRQ
monitor (``cat /proc/irqs``).  The ISA is rv32imac/ilp32.

Building
========

With make::

  $ ./tools/configure.sh mbv-qemu:nsh
  $ make -j

With CMake::

  $ cmake -B build -DBOARD_CONFIG=mbv-qemu:nsh -GNinja
  $ cmake --build build

Any RISC-V bare metal GCC with an rv32 multilib for the selected ISA can be
used (e.g. ``riscv-none-elf-gcc`` or ``riscv64-unknown-elf-gcc``).

Running
=======

QEMU must provide the machine (``qemu-system-riscv32 -M help | grep
microblaze``); it was tested with QEMU 11.1.2.  The machine ignores
``-kernel``, so the image is loaded with the generic loader, which also sets
the PC to the ELF entry point when ``cpu-num`` is given.  The board script
does this::

  $ ./boards/risc-v/mbv/mbv-qemu/scripts/run-qemu.sh nuttx

which runs::

  $ qemu-system-riscv32 -M amd-microblaze-v-generic -m 256M \
      -cpu rv32i,zicsr=true,zifencei=true,m=true,a=true,c=true,f=false \
      -display none -serial null -serial mon:stdio \
      -device loader,file=nuttx,cpu-num=0 -d unimp,guest_errors

The first serial port is the AXI UART Lite and the second one the AXI
UART16550, hence ``-serial null -serial mon:stdio``.  Exit QEMU with
``Ctrl-a x``.

.. warning::

   The default QEMU RISC-V CPU implements many more extensions than a
   MicroBlaze V (F, D, Zba, Zbb, ...).  ``run-qemu.sh`` starts from the
   bare ``rv32i`` CPU and enables only Zicsr, Zifencei and the M/A/C/F
   extensions that are enabled in ``.config``, so an instruction the real
   core lacks traps in QEMU as well.  Keep this in mind when running QEMU
   by hand.

Debugging
=========

Start QEMU halted and waiting for GDB on port 1234::

  $ EXTRA="-s -S" ./boards/risc-v/mbv/mbv-qemu/scripts/run-qemu.sh nuttx

and connect::

  $ riscv64-unknown-elf-gdb nuttx
  (gdb) target remote :1234

With ``CONFIG_DEBUG_FEATURES`` the boot code prints ``A`` and ``B`` through
the polled ``up_putc()`` before the interrupt controller is initialized.

Known Issues
============

The QEMU AXI Timer model restarts the period relative to the moment its
host timer callback runs.  On hosts with coarse timers (observed on macOS)
every tick interrupt is therefore late by the host timer latency, about
80 instead of 100 interrupts per second.  ``CONFIG_MBV_TIMER_TICK_CATCHUP``
compensates for this using counter 1 as the time reference, so the system
time and ``sleep`` stay correct (``sleep 5`` takes 5 seconds), but
``/proc/irqs`` shows the lower interrupt rate of the timer.  Without that
option ``sleep 5`` takes about 6 seconds on such hosts.

Real Hardware
=============

The address map and the AXI INTC input order above are the Vivado address
editor defaults, so a block design that keeps them runs this configuration
unchanged.  Otherwise update ``CONFIG_MBV_INTC_*``, ``CONFIG_MBV_TIMER_*``,
``CONFIG_16550_UART0_*``, ``CONFIG_RAM_START``/``CONFIG_RAM_SIZE`` and the
board ``board.h`` to the design.  In particular:

- Match ``CONFIG_MBV_ISA_*`` to the MicroBlaze V configuration.
- Set ``CONFIG_MBV_TIMER_CLK_FREQ`` and ``CONFIG_16550_UART0_CLOCK`` to the
  AXI clock (``s_axi_aclk``).
- Set ``CONFIG_MBV_INTC_EDGE_MASK`` to ``C_KIND_OF_INTR`` and
  ``CONFIG_MBV_INTC_NIRQS`` to ``C_NUM_INTR_INPUTS`` of the AXI INTC.
- The image is linked to DDR and has to be loaded by the debugger (e.g.
  ``dow nuttx`` in XSDB or ``load`` in GDB); it does not boot from BRAM.
