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
AXI UART Lite          0x40600000    1 (edge)   Console, see uartlite
AXI UART16550          0x44a11000    4          Console (other configs)
AXI EthernetLite       0x40e00000    5          ``eth0``, see netnsh
AXI Timer #2           0x41c10000    6          Not used
AXI Ethernet           0x40c00000    7          Not supported
AXI DMA                0x41e00000    9 / 8      Not supported (MM2S / S2MM)
AXI GPIO x3            0x40000000    \-         Not implemented by QEMU
                       0x40010000
                       0x40020000
AXI IIC                0x40800000    \-         Not implemented by QEMU
AXI Quad SPI           0x44a00000    2          Patched QEMU only, see spiflash
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

uartlite
--------

``nsh`` with the AXI UART Lite driver (``CONFIG_MBV_UARTLITE0``) as the
console instead of the AXI UART16550, which is disabled.  This is the
console of the Arty A7 design.  The UART Lite is ``/dev/ttyS0``; ``cat
/proc/irqs`` shows its interrupt as IRQ 28 (AXI INTC input 1).

spiflash
--------

``nsh`` plus the AXI Quad SPI driver (``CONFIG_MBV_SPI0``) with a Micron
N25Q128A13 serial flash on slave select 0, the configuration flash of the
Arty A7.  The flash is ``/dev/mtd0`` (``m25px`` driver) and is mounted on
``/mnt/flash`` with LittleFS, formatted on first use.  ``/dev/spi0`` and
the ``spi`` tool give raw access::

  nsh> spi exch -x 4 9F000000
  Sending:	9F 00 00 00
  Received:	00 20 BA 18
  nsh> echo hello > /mnt/flash/test.txt

The upstream QEMU machine has no SPI controller at 0x44a00000.  This
configuration needs a QEMU with the Xilinx XPS SPI model (``xlnx.xps-spi``,
which implements the AXI Quad SPI standard mode registers) and an
``n25q128a13`` flash added to ``amd-microblaze-v-generic``, on AXI INTC
input 2.  Run it with a 16 MiB flash image, created if missing::

  $ QEMU=/path/to/qemu-system-riscv32 FLASH=flash.img \
      ./boards/risc-v/mbv/mbv-qemu/scripts/run-qemu.sh nuttx

netnsh
------

``nsh`` plus the AXI EthernetLite driver (``CONFIG_MBV_ETHLITE``) and the
IPv4 stack with TCP, UDP and ICMP.  The address matches QEMU user
networking: 10.0.2.15/24, gateway 10.0.2.2, DNS 10.0.2.3.  ``ping``,
``nslookup``, ``wget`` and ``telnetd`` (port 23) are included::

  nsh> ping -c 3 10.0.2.2
  56 bytes from 10.0.2.2: icmp_seq=0 time=10.0 ms
  ...
  nsh> nslookup example.com

The upstream QEMU machine connects the EthernetLite to user networking
without any option.  To reach telnetd from the host, forward a port::

  $ HOSTFWD="tcp:127.0.0.1:2323-:23" \
      ./boards/risc-v/mbv/mbv-qemu/scripts/run-qemu.sh nuttx
  $ telnet 127.0.0.1 2323

QEMU then warns that the AXI Ethernet NIC has no peer; it is not used.

full
----

The peripherals of the Arty A7 design together: the ``uartlite`` console,
the ``netnsh`` network stack with ``telnetd`` and the ``spiflash`` SPI flash
with LittleFS on ``/mnt/flash``.  ``cat /proc/irqs`` shows the UART Lite as
IRQ 28 and the EthernetLite as IRQ 32.  Like ``spiflash``, it needs a QEMU
with the AXI Quad SPI model::

  $ QEMU=/path/to/qemu-system-riscv32 FLASH=flash.img \
      HOSTFWD="tcp:127.0.0.1:2323-:23" \
      ./boards/risc-v/mbv/mbv-qemu/scripts/run-qemu.sh nuttx
  $ telnet 127.0.0.1 2323

The serial console and the telnet session are two independent NSH
sessions, and files written to ``/mnt/flash`` are kept in ``flash.img``.

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
UART16550.  The script connects the console selected in ``.config`` to
stdio and the other port to null: ``-serial null -serial mon:stdio`` for
the 16550, ``-serial mon:stdio -serial null`` with
``CONFIG_MBV_UARTLITE0_SERIAL_CONSOLE``.  Exit QEMU with ``Ctrl-a x``.

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
``CONFIG_16550_UART0_*`` or ``CONFIG_MBV_UARTLITE0_*``, ``CONFIG_RAM_START``/``CONFIG_RAM_SIZE`` and the
board ``board.h`` to the design.  In particular:

- Match ``CONFIG_MBV_ISA_*`` to the MicroBlaze V configuration.
- Set ``CONFIG_MBV_TIMER_CLK_FREQ`` and ``CONFIG_16550_UART0_CLOCK`` to the
  AXI clock (``s_axi_aclk``).
- Set ``CONFIG_MBV_INTC_EDGE_MASK`` to ``C_KIND_OF_INTR`` and
  ``CONFIG_MBV_INTC_NIRQS`` to ``C_NUM_INTR_INPUTS`` of the AXI INTC.
- The image is linked to DDR and has to be loaded by the debugger (e.g.
  ``dow nuttx`` in XSDB or ``load`` in GDB); it does not boot from BRAM.
