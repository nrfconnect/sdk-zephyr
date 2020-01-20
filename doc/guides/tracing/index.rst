.. _tracing:

Tracing
#######

Overview
********

The tracing feature provides hooks that permits you to collect data from
your application and allows enabled backends to visualize the inner-working of
the kernel and various subsystems.

Applications and tracing tools can create a backend that redefines the
macros declared in :zephyr_file:`include/debug/tracing.h` that are called
across the kernel in key spots.

.. doxygengroup:: tracing_apis
   :project: Zephyr

SEGGER SystemView Support
*************************

Zephyr provides built-in support for `SEGGER SystemView`_ that can be enabled in
any application for platforms that have the required hardware support.

To enable tracing support with `SEGGER SystemView`_ add the configuration option
:option:`CONFIG_SEGGER_SYSTEMVIEW` to your project configuration file and set
it to *y*. For example, this can be added to the
:ref:`synchronization_sample` to visualize fast switching between threads::

    CONFIG_STDOUT_CONSOLE=y
    # enable to use thread names
    CONFIG_THREAD_NAME=y
    CONFIG_SEGGER_SYSTEMVIEW=y
    CONFIG_USE_SEGGER_RTT=y


.. figure:: segger_systemview.png
    :align: center
    :alt: SEGGER SystemView
    :figclass: align-center
    :width: 80%

.. _SEGGER SystemView: https://www.segger.com/products/development-tools/systemview/

.. _ctf:

Common Trace Format (CTF) Support
*********************************

Common Trace Format, CTF, is an open format and language to describe trace
formats. This enables tool reuse, of which line-textual (babeltrace) and
graphical (TraceCompass) variants already exist.

CTF should look familiar to C programmers but adds stronger typing.
See `CTF - A Flexible, High-performance Binary Trace Format
<http://diamon.org/ctf/>`_.

Every system has application-specific events to trace out.  Historically,
that has implied:

1. Determining the application-specific payload,
2. Choosing suitable serialization-format,
3. Writing the on-target serialization code,
4. Deciding on and writing the I/O transport mechanics,
5. Writing the PC-side deserializer/parser,
6. Writing custom ad-hoc tools for filtering and presentation.

CTF allows us to formally describe #1 and #2, which enables common
infrastructure for #5 and #6.  This leaves #3 serialization code and #4
I/O mechanics up to a custom implementation.

This CTF debug module aims at providing a common #1 and #2 for Zephyr
("top"), while providing a lean & generic interface for I/O ("bottom").
Currently, only one CTF bottom-layer exists, POSIX ``fwrite``, but many others
are possible:

- Async UART
- Async DMA
- Sync GPIO
- ... and many more.

In fact, I/O varies greatly from system to system.  Therefore, it is
instructive to create a taxonomy for I/O types when we must ensure the
interface between CTF-top and CTF-bottom is generic and efficient
enough to model these. See the *I/O taxonomy* section below.


A Generic Interface
====================

In CTF, an event is serialized to a packet containing one or more fields.
As seen from *I/O taxonomy* section below, a bottom layer may:

- perform actions at transaction-start (e.g. mutex-lock),
- process each field in some way (e.g. sync-push emit, concat, enqueue to
  thread-bound FIFO),
- perform actions at transaction-stop (e.g. mutex-release, emit of concat
  buffer).

The bottom-layer then needs to implement the following macros:

- ``CTF_BOTTOM_LOCK``:   No-op or how to lock the I/O transaction
- ``CTF_BOTTOM_UNLOCK``: No-op or how to release the I/O transaction
- ``CTF_BOTTOM_FIELDS``: Var-args of fields. May process each field with ``MAP``
- ``CTF_BOTTOM_TIMESTAMPED_INTERNALLY``: Tells where timestamping is done

These macros along with inline functions of the top-layer can yield a
very low-overhead tracing infrastructure.


CTF Top-Layer Example
=========================

The CTF_EVENT macro will serialize each argument to a field::

  /* Example for illustration */
  static inline void ctf_top_foo(u32_t thread_id, ctf_bounded_string_t name)
  {
    CTF_EVENT(
      CTF_LITERAL(u8_t, 42),
      thread_id,
      name,
      "hello, I was emitted from function: ",
      __func__  /* __func__ is standard since C99 */
    );
  }

How to serialize and emit fields as well as handling alignment, can be done
internally and statically at compile-time in the bottom-layer.


How to Activate?
================

Make sure :option:`CONFIG_TRACING_CTF` is set to ``y`` (:option:`CONFIG_TRACING_CTF_BOTTOM_POSIX`
is selected by default when using ``BOARD_NATIVE_POSIX``).


How to Use?
===========

The resulting CTF output can be visualized using babeltrace or TraceCompass:

- The CTF output file can be specified in native posix using the ``-ctf-path``
  command line option

- Create a new empty directory and copy into it:

  - The TSDL file (``subsys/debug/tracing/ctf/tsdl/metadata``)

  - The CTF output file renaming it to ``channel0_0``

- The trace can be opened by pointing TraceCompass or babeltrace to this new
  directory


What is TraceCompass?
=====================

TraceCompass is an open source tool that visualizes CTF events such as thread
scheduling and interrupts, and is helpful to find unintended interactions and
resource conflicts on complex systems.

See also the presentation by Ericsson,
`Advanced Trouble-shooting Of Real-time Systems
<https://wiki.eclipse.org/images/0/0e/TechTalkOnlineDemoFeb2017_v1.pdf>`_.


Future LTTng Inspiration
========================

Currently, the top-layer provided here is quite simple and bare-bones,
and needlessly copied from Zephyr's Segger SystemView debug module.

For an OS like Zephyr, it would make sense to draw inspiration from
Linux's LTTng and change the top-layer to serialize to the same format.
Doing this would enable direct reuse of TraceCompass' canned analyses
for Linux.  Alternatively, LTTng-analyses in TraceCompass could be
customized to Zephyr.  It is ongoing work to enable TraceCompass
visibility of Zephyr in a target-agnostic and open source way.


I/O Taxonomy
=============

- Atomic Push/Produce/Write/Enqueue:

  - synchronous:
                  means data-transmission has completed with the return of the
                  call.

  - asynchronous:
                  means data-transmission is pending or ongoing with the return
                  of the call. Usually, interrupts/callbacks/signals or polling
                  is used to determine completion.

  - buffered:
                  means data-transmissions are copied and grouped together to
                  form a larger ones. Usually for amortizing overhead (burst
                  dequeue) or jitter-mitigation (steady dequeue).

  Examples:
    - sync  unbuffered
        E.g. PIO via GPIOs having steady stream, no extra FIFO memory needed.
        Low jitter but may be less efficient (cant amortize the overhead of
        writing).

    - sync  buffered
        E.g. ``fwrite()`` or enqueuing into FIFO.
        Blockingly burst the FIFO when its buffer-waterlevel exceeds threshold.
        Jitter due to bursts may lead to missed deadlines.

    - async unbuffered
        E.g. DMA, or zero-copying in shared memory.
        Be careful of data hazards, race conditions, etc!

    - async buffered
        E.g. enqueuing into FIFO.



- Atomic Pull/Consume/Read/Dequeue:

  - synchronous:
                  means data-reception has completed with the return of the call.

  - asynchronous:
                  means data-reception is pending or ongoing with the return of
                  the call. Usually, interrupts/callbacks/signals or polling is
                  used to determine completion.

  - buffered:
                  means data is copied-in in larger chunks than request-size.
                  Usually for amortizing wait-time.

  Examples:
    - sync  unbuffered
        E.g. Blocking read-call, ``fread()`` or SPI-read, zero-copying in shared
        memory.

    - sync  buffered
        E.g. Blocking read-call with caching applied.
        Makes sense if read pattern exhibits spatial locality.

    - async unbuffered
        E.g. zero-copying in shared memory.
        Be careful of data hazards, race conditions, etc!

    - async buffered
        E.g. ``aio_read()`` or DMA.



Unfortunately, I/O may not be atomic and may, therefore, require locking.
Locking may not be needed if multiple independent channels are available.

  - The system has non-atomic write and one shared channel
        E.g. UART. Locking required.

        ``lock(); emit(a); emit(b); emit(c); release();``

  - The system has non-atomic write but many channels
        E.g. Multi-UART. Lock-free if the bottom-layer maps each Zephyr
        thread+ISR to its own channel, thus alleviating races as each
        thread is sequentially consistent with itself.

        ``emit(a,thread_id); emit(b,thread_id); emit(c,thread_id);``

  - The system has atomic write     but one shared channel
        E.g. ``native_posix`` or board with DMA. May or may not need locking.

        ``emit(a ## b ## c); /* Concat to buffer */``

        ``lock(); emit(a); emit(b); emit(c); release(); /* No extra mem */``

  - The system has atomic write     and many channels
        E.g. native_posix or board with multi-channel DMA. Lock-free.

        ``emit(a ## b ## c, thread_id);``
