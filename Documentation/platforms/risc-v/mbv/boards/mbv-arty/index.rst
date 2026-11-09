========
mbv-arty
========

.. tags:: arch:risc-v, chip:mbv, vendor:amd

MicroBlaze V in the ``soc_bd`` Vivado block design on a Digilent Arty
A7-100T (xc7a100tcsg324-1), bitstream v0.2.  The design only has the IPs
listed below; there is no AXI UART16550, AXI EthernetLite, AXI GPIO or AXI
IIC, and the AXI Quad SPI drives a microSD card on Pmod JA rather than the
configuration flash.  On QEMU the board runs on the ``arty-a7-mbv`` machine,
which has the same IPs, the same ISA (rv32imac_zicsr_zifencei, M-mode) and
resets to the LMB BRAM.

Memory Map and Interrupts
=========================

=====================  ============  =========  ==============================
IP                     Base          INTC in    NuttX
=====================  ============  =========  ==============================
LMB BRAM (256 KiB)     0x00000000    \-         Image of ``nsh``, ``sdcard``
DDR3L (256 MiB)        0x80000000    \-         Image of ``nsh-ddr``
AXI UART Lite          0x40600000    1 (edge)   Console, ``/dev/ttyS0``
AXI INTC (3 inputs)    0x41200000    \-         Output to MEIP
AXI Timer              0x41c00000    0          System tick
AXI Quad SPI           0x44a00000    2          SD card, ``/dev/mmcsd0``
=====================  ============  =========  ==============================

The INTC inputs, the Quad SPI settings (standard mode, 256 byte FIFOs, SCK
fixed to 5 MHz / 16 = 312.5 kHz) and the clocks come from the block design.
The INTC edge mask (``CONFIG_MBV_INTC_EDGE_MASK``, UART Lite only) and
``C_HAS_IVR`` are IP defaults that still have to be read from the hardware
handoff (``.hwh``).  The board checks at build time that the UART Lite,
Quad SPI and INTC Kconfig values match ``include/board.h``.

The image is linked to ``CONFIG_RAM_START`` (the LMB BRAM or the DDR) with
size ``CONFIG_RAM_SIZE``; code, data, the idle stack and the heap all live
there.  ``.data`` is not copied from a load address, so a CPU reset without
reloading the image (or the bitstream) keeps the values of the previous run.

``__start`` begins with the MicroBlaze V vector table: reset at
``C_BASE_VECTORS``, then jumps to the trap handler at +0x4 (the reset value
of ``mtvec``), +0x8 (non-maskable interrupt) and +0xc (non-maskable
exception), so that a trap before ``mtvec`` is set does not land in the
startup code.

``CONFIG_MBV_TIMER_TICK_CATCHUP`` is disabled: the AXI Timer interrupt is
not late on the board.  On QEMU on a macOS host the tick can then drift and
``sleep`` may take longer than requested.

The ``CONFIG_DEFAULT_SMALL`` configurations enable
``CONFIG_LIBC_TEMPBUFFER``.  Without it ``lib_get_tempbuffer()`` is
``alloca()``, and the path buffer that ``inode_search_setup()`` allocates
with it is gone when it returns: the relative path handed to a mounted file
system is then overwritten by the next calls (seen as ``EINVAL`` from FAT
when creating a file).

Configurations
==============

nsh
---

Boots from the LMB BRAM: entry ``0x0``, the reset vector, so the ELF can be
embedded in the bitstream with ``updatemem``.  Everything must fit into
``CONFIG_RAM_SIZE`` (256 KiB), otherwise the link fails with ``region `bram'
overflowed``; the linker script also checks that ``__start`` is at ``0x0``.

It is a ``CONFIG_DEFAULT_SMALL`` build with the commands needed for board
bring-up: ``cat``, ``echo``, ``free``, ``ls``, ``mw``, ``xd``, ``sleep``,
``uname``, ``uptime``, ``/proc/irqs`` (IRQ monitor) and register dumps on
exceptions (``CONFIG_ARCH_STACKDUMP``).  About 200 KiB of the BRAM are left
for the heap::

     text    data     bss     dec     hex filename
    50356     813    3104   54273    d401 nuttx

sdcard
------

``nsh`` plus the AXI Quad SPI driver (``CONFIG_MBV_SPI0``) and the MMC/SD
SPI driver for the microSD card on slave select 0 (HW-125 module on Pmod
JA), also from the LMB BRAM (about 80 KiB).  The card is ``/dev/mmcsd0`` and,
if it holds a FAT file system, is mounted on ``/mnt/sd``
(``CONFIG_MBV_ARTY_SD_MOUNTPOINT``) at boot, with long file names.

The Quad SPI has no card detect input, so the driver reports an MMC/SD card
as present and the MMC/SD driver finds out from CMD0 whether one answers.
Before binding the card the board sends 80 clocks with the card selected:
the AXI Quad SPI does not clock SCK while every slave select output is
inactive, so the power-up clocks the MMC/SD driver sends with the card
deselected never reach it on the board.

nsh-ddr
-------

Full featured NSH with ``ostest``, linked to the DDR at ``0x80000000``.  On
the board it is loaded over JTAG (``xsdb``: ``dow nuttx`` and ``con``), the
DDR has no content after configuration.

Running on QEMU
===============

The ``arty-a7-mbv`` machine is added by a QEMU patch (see the project
repository).  It needs no ``-m``, ``-cpu`` or second ``-serial``.  The BRAM
images start from the reset vector like on the board; DDR images need
``cpu-num=0`` to set the entry point::

  $ qemu-system-riscv32 -M arty-a7-mbv -display none -serial mon:stdio \
      -device loader,file=nuttx -drive if=sd,format=raw,file=sd.img
  $ qemu-system-riscv32 -M arty-a7-mbv -display none -serial mon:stdio \
      -device loader,file=nuttx,cpu-num=0

``sd.img`` is a raw SD card image whose size is a power of two.  Accesses
to addresses without an IP fault on QEMU (load/store access fault); on the
board the result depends on the AXI interconnect.

Running on the Board
====================

``CONFIG_BOARD_LOOPSPERMSEC`` is copied from ``mbv-qemu`` and has to be
calibrated on the board.  The console is the USB UART of the Arty at 115200
8N1, fixed in the UART Lite core.  The HW-125 module needs 5 V on its VCC
pin (its 3.3 V regulator does not work from 3.3 V).
