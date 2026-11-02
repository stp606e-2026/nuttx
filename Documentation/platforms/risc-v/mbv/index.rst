================
AMD MicroBlaze V
================

.. tags:: arch:risc-v, chip:mbv, vendor:amd

The AMD MicroBlaze V is a RISC-V soft processor for AMD FPGAs.  It is
configured in Vivado and is usually combined with AMD AXI peripherals
(AXI INTC, AXI Timer, AXI UART16550, AXI UART Lite, AXI GPIO, ...).

The ``mbv`` chip (``CONFIG_ARCH_CHIP_MBV``) supports the 32-bit core in
machine mode with a flat build.  The chip layer only depends on the core
and on the two AXI IPs needed to run NuttX (AXI INTC and AXI Timer), so it
can be used with any block design that contains them.

Core
====

- RV32I + Zicsr + Zifencei.  The M, A, C and F extensions are optional in
  Vivado and are selected with ``CONFIG_MBV_ISA_M``, ``CONFIG_MBV_ISA_A``,
  ``CONFIG_MBV_ISA_C`` and ``CONFIG_MBV_ISA_F`` (default: rv32imac, ABI
  ilp32; with F and ``CONFIG_ARCH_FPU`` the ABI is ilp32f).
- The core starts in M-mode.  NuttX runs in M-mode only; S/U-mode, PMP,
  ``BUILD_PROTECTED``/``BUILD_KERNEL`` and SMP are not supported.
- There is a single external interrupt line (MEIP).  There is no CLINT,
  ACLINT or PLIC and no ``mtime``/``mtimecmp``/``time`` CSR, so the common
  RISC-V mtimer and the ``mtime`` based perf/time code are not used.
- The trap vector is the common ``__trap_vec`` in direct mode.

.. warning::

   The ISA selected in Kconfig must match the core generated in Vivado.
   An image that uses an extension the core does not implement fails with
   an illegal instruction exception.

   The toolchain must also provide a multilib (``libgcc``) for the chosen
   ``-march``/``-mabi`` combination.  For example the GNU ``riscv64-elf``
   toolchain from Homebrew ships ``rv32i``, ``rv32im``, ``rv32iac``,
   ``rv32imac`` and ``rv32imafc``; check with
   ``riscv64-unknown-elf-gcc -print-multi-lib``.

Peripherals
===========

=================  =========  ====================================================
Peripheral         Support    Notes
=================  =========  ====================================================
AXI INTC (PG099)   Yes        Chip layer, drives MEIP
AXI Timer (PG079)  Yes        Counter 0: tick IRQ, counter 1: time reference
AXI UART16550      Yes        Generic ``drivers/serial/uart_16550.c``
AXI UART Lite      No
AXI GPIO           No
AXI Quad SPI       Yes        SPI master, standard mode, 8 bit, polled
AXI IIC            No
AXI EthernetLite   Yes        ``eth0``, netdev lower half, RX/TX interrupt driven
=================  =========  ====================================================

Interrupts
==========

AXI INTC input ``n`` is the NuttX IRQ ``RISCV_IRQ_EXT + n``
(``MBV_IRQ_INTC(n)`` in ``<arch/irq.h>``) and
``NR_IRQS = RISCV_IRQ_EXT + CONFIG_MBV_INTC_NIRQS``.  Input 0 has the same
number as ``RISCV_IRQ_EXT``: the MEIE bit is owned by the chip layer and is
never changed by ``up_enable_irq()``/``up_disable_irq()``, which act on the
AXI INTC (SIE/CIE) for external IRQs and on ``mie`` for the core
interrupts.

On a machine external interrupt the dispatcher reads IVR until it reports
no pending input (all ones) and calls the handler of each pending input:

- Level-triggered inputs are acknowledged in IAR *after* the handler, which
  must clear the request at the source first.
- Edge-triggered inputs (bits set in ``CONFIG_MBV_INTC_EDGE_MASK``, i.e.
  ``C_KIND_OF_INTR``) are acknowledged *before* the handler, so that an
  edge arriving while the handler runs is not lost.

``up_irqinitialize()`` sets ``MER = ME | HIE``.  After that, ISR can no
longer be written by software and HIE stays set until the next reset.

System Tick
===========

Counter 0 of the AXI Timer runs in generate mode, counting down with auto
reload.  The period of a down counter is ``TLR + 2`` clocks, so
``TLR = CONFIG_MBV_TIMER_CLK_FREQ / TICK_PER_SEC - 2``.  The timer interrupt
is level triggered; the handler writes 1 to T0INT before returning.
``CONFIG_MBV_TIMER_CLK_FREQ`` should be a multiple of the tick rate,
otherwise the tick is slightly off (a build warning is emitted).

With ``CONFIG_MBV_TIMER_TICK_CATCHUP`` (default), counter 1 of the same AXI
Timer runs as a free running up counter without interrupt and is the time
reference.  On each counter 0 interrupt, one tick is processed for every
tick period that has elapsed on counter 1, so a tick interrupt delayed by
more than one period (or lost) does not slow down the system time.  On
real hardware this processes exactly one tick per interrupt.  Disable it
if the AXI Timer has a single counter (``C_ONE_TIMER_ONLY``).

AXI EthernetLite
================

``CONFIG_MBV_ETHLITE`` registers the AXI EthernetLite (PG135) as ``eth0``
through the network device lower half (``netdev_lowerhalf.h``).  Frames
are copied to and from the core's packet buffers with 32-bit accesses
only.  ``ifup`` loads the MAC address into the core (TX control
``PROGRAM | STATUS``), which then drops received unicast frames for other
addresses.

- **RX:** the core has no RX length register.  The frame length is taken
  from the IPv4 or IPv6 header (or is fixed for ARP, or comes from the
  802.3 length field), other frames are read in full (1514 bytes).  The
  RX buffer is handed back to the core as soon as the frame is copied.
- **TX:** each TX buffer holds one packet until its STATUS bit clears; the
  TX quota is the number of TX buffers, and ``reclaim`` frees the sent
  packets.
- **Interrupt:** one interrupt for RX and TX done.  The handler requests
  an RX poll and, if packets are in flight, a TX done on the low priority
  work queue (``CONFIG_SCHED_LPWORK`` is required).
- **Ping-pong buffers:** ``CONFIG_MBV_ETHLITE_TX_PINGPONG`` and
  ``CONFIG_MBV_ETHLITE_RX_PINGPONG`` must match ``C_TX_PING_PONG`` and
  ``C_RX_PING_PONG``.  QEMU's model is used without them.
- **MDIO/PHY:** not used, the carrier is always on.  On a board the PHY
  auto-negotiates on its own.

On hardware the EthernetLite interrupt is a rising edge
(``SENSITIVITY EDGE_RISING``), so its bit must be set in
``CONFIG_MBV_INTC_EDGE_MASK``.  QEMU pulses a level-triggered input, which
the default mask already handles.

Kconfig Options
===============

===================================  ============  ==============================================
Option                               Default       Description
===================================  ============  ==============================================
``CONFIG_MBV_ISA_M``                 y             M extension
``CONFIG_MBV_ISA_A``                 y             A extension
``CONFIG_MBV_ISA_C``                 y             C extension
``CONFIG_MBV_ISA_F``                 n             F extension (enables ``ARCH_HAVE_FPU``)
``CONFIG_MBV_INTC_BASE``             0x41200000    AXI INTC base address
``CONFIG_MBV_INTC_NIRQS``            32            AXI INTC inputs (``C_NUM_INTR_INPUTS``)
``CONFIG_MBV_INTC_EDGE_MASK``        0x00000002    Edge-triggered inputs (``C_KIND_OF_INTR``)
``CONFIG_MBV_INTC_HAS_IVR``          y             ``C_HAS_IVR``; otherwise IPR is scanned
``CONFIG_MBV_INTC_HAS_SIE_CIE``      y             ``C_HAS_SIE``/``C_HAS_CIE``; otherwise IER RMW
``CONFIG_MBV_TIMER_BASE``            0x41c00000    AXI Timer used for the tick
``CONFIG_MBV_TIMER_IRQ``             0             AXI INTC input of that timer
``CONFIG_MBV_TIMER_CLK_FREQ``        100000000     AXI Timer clock (``s_axi_aclk``)
``CONFIG_MBV_TIMER_TICK_CATCHUP``    y             Counter 1 as time reference (lost ticks)
``CONFIG_MBV_ETHLITE``               n             AXI EthernetLite driver (``eth0``)
``CONFIG_MBV_ETHLITE_BASE``          0x40e00000    AXI EthernetLite base address
``CONFIG_MBV_ETHLITE_IRQ``           5             AXI INTC input of the EthernetLite
``CONFIG_MBV_ETHLITE_TX_PINGPONG``   n             ``C_TX_PING_PONG``
``CONFIG_MBV_ETHLITE_RX_PINGPONG``   n             ``C_RX_PING_PONG``
``CONFIG_MBV_ETHLITE_MACADDR_1/2``   00:0a:35:...  MAC address (default 00:0a:35:00:01:22)
===================================  ============  ==============================================

The defaults are the Vivado address editor defaults, which are also used by
the QEMU ``amd-microblaze-v-generic`` machine.

Adding Drivers
==============

Chip level drivers for AXI IPs go to ``arch/risc-v/src/mbv/`` as
``mbv_<ip>.c`` with the register definitions in
``arch/risc-v/src/mbv/hardware/mbv_<ip>.h``.  Base addresses and AXI INTC
inputs are not hard coded in the chip layer: they come from Kconfig or from
the board (``boards/risc-v/mbv/<board>/include/board.h``), and the board
registers the device in its ``board_late_initialize()``.  Register access
goes through ``getreg32()``/``putreg32()`` so that it can be mocked in host
side unit tests.

Supported Boards
================

.. toctree::
   :glob:
   :maxdepth: 1

   boards/*/*
