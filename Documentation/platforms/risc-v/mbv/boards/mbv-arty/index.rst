========
mbv-arty
========

.. tags:: arch:risc-v, chip:mbv, vendor:amd

MicroBlaze V in the ``soc_bd`` Vivado block design on a Digilent Arty
A7-100T (xc7a100tcsg324-1).  The design only has the IPs listed below; there
is no AXI UART16550, AXI EthernetLite, AXI GPIO or AXI IIC.  On QEMU the
board runs on the ``arty-a7-mbv`` machine, which has the same IPs, the same
ISA (rv32imac_zicsr_zifencei, M-mode) and resets to the LMB BRAM.

Memory Map and Interrupts
=========================

=====================  ============  =========  ==============================
IP                     Base          INTC in    NuttX
=====================  ============  =========  ==============================
LMB BRAM (64 KiB)      0x00000000    \-         Image of ``nsh``
DDR3L (256 MiB)        0x80000000    \-         Image of the other configs
AXI UART Lite          0x40600000    1 (edge)   Console, ``/dev/ttyS0``
AXI INTC               0x41200000    \-         Output to MEIP
AXI Timer              0x41c00000    0          System tick
AXI Quad SPI           0x44a00000    2          Configuration flash
=====================  ============  =========  ==============================

Only the AXI Timer input is verified against the design so far.  The UART
Lite and Quad SPI inputs, the INTC edge mask (``CONFIG_MBV_INTC_EDGE_MASK``,
UART Lite only), ``C_HAS_IVR``, the number of INTC inputs and the 100 MHz
clock still have to be read from the hardware handoff (``.hwh``).  The board
checks at build time that the UART Lite and Quad SPI Kconfig values match
``include/board.h``.

The image is linked to ``CONFIG_RAM_START`` (the LMB BRAM or the DDR) with
size ``CONFIG_RAM_SIZE``; code, data, the idle stack and the heap all live
there.  ``.data`` is not copied from a load address, so a CPU reset without
reloading the image (or the bitstream) keeps the values of the previous run.

``CONFIG_MBV_TIMER_TICK_CATCHUP`` is disabled: the AXI Timer interrupt is
not late on the board.  On QEMU on a macOS host the tick can then drift and
``sleep`` may take longer than requested; ``mbv-qemu`` keeps the catch-up
enabled for that reason.

Configurations
==============

nsh
---

Boots from the LMB BRAM: entry ``0x0``, the reset vector, so the ELF can be
embedded in the bitstream with ``updatemem``.  Everything must fit into
``CONFIG_RAM_SIZE`` (64 KiB), otherwise the link fails with ``region `bram'
overflowed``; the linker script also checks that ``__start`` is at ``0x0``.
To follow the BRAM when it is enlarged in Vivado (e.g. to 256 KiB), only
``CONFIG_RAM_SIZE`` changes.

It is a ``CONFIG_DEFAULT_SMALL`` build with the commands needed for board
bring-up: ``cat``, ``echo``, ``free``, ``ls``, ``mw``, ``xd``, ``sleep``,
``uname``, ``uptime``, ``/proc/irqs`` (IRQ monitor) and register dumps on
exceptions (``CONFIG_ARCH_STACKDUMP``).  There is no ``ps`` and no
``ostest``.  Size of the image (64 KiB BRAM, about 10 KiB left for the
heap)::

     text    data     bss     dec     hex filename
    50116     297    4032   54445    d4ad nuttx

nsh-ddr
-------

Full featured NSH with ``ostest``, linked to the DDR at ``0x80000000``.  On
the board it is loaded over JTAG (``xsdb``: ``dow nuttx`` and ``con``), the
DDR has no content after configuration.

spiflash
--------

``nsh-ddr`` plus the AXI Quad SPI driver (``CONFIG_MBV_SPI0``) and the
Micron N25Q128A13 configuration flash on slave select 0.  The flash also
holds the bitstream (about 3.8 MB for the xc7a100t) at offset 0, so only the
part from ``CONFIG_MBV_ARTY_FLASH_FS_OFFSET`` (4 MiB) to the end is
registered as ``/dev/mtd0`` and mounted on ``/mnt/flash`` with LittleFS
(formatted on first use, 12 MiB).  There is no ``/dev/spi0`` or ``spi`` tool,
which could erase the bitstream.

Running on QEMU
===============

The ``arty-a7-mbv`` machine is added by a QEMU patch (see the project
repository).  It needs no ``-m``, ``-cpu`` or second ``-serial``.  The BRAM
image starts from the reset vector like on the board; DDR images need
``cpu-num=0`` to set the entry point::

  $ qemu-system-riscv32 -M arty-a7-mbv -display none -serial mon:stdio \
      -device loader,file=nuttx
  $ qemu-system-riscv32 -M arty-a7-mbv -display none -serial mon:stdio \
      -device loader,file=nuttx,cpu-num=0 \
      -drive if=mtd,format=raw,file=flash.img

``flash.img`` is a 16 MiB image.  Accesses to addresses without an IP fault
on QEMU (load/store access fault); on the board the result depends on the
AXI interconnect.

Running on the Board
====================

``CONFIG_BOARD_LOOPSPERMSEC`` is copied from ``mbv-qemu`` and has to be
calibrated on the board.  The console is the USB UART of the Arty at 115200
8N1, fixed in the UART Lite core.
