.. include:: replace.txt

Multi-threaded Parallel Simulation (MTP)
----------------------------

This module provides a fast and user-transparent parallel simulator
implementation for ns-3. By splitting up the to-be-simulated topology
into multiple logical processes, LPs, with fine granularity, each LP
can be dynamically scheduled and processed by a thread for load balancing,
while reducing cache misses. With this approach, siginficant speedup
can be acheived for large topologies with heavy traffic.

.. _current-implementation-details:

Current Implementation Details
******************************

This module contains three parts: A parallel simulator implementation
``MultithreadedSimulatorImpl``, an interface to users ``MtpInterface``,
and ``LogicalProcess`` to represent LPs in terms of parallel simulation.

All LPs and threads are stored in the ``MtpInterface``. It controls the
simulation progress, schedules LPs to threads and manages the lifecycles
of LPs and threads. The interface also provides some methods and options
for users to tweak the simulation.

Each LP's logic is implemented in ``LogicalProcess``. It contains most of
the methods of the default sequential simulator plus some auxiliary methods
for parallel simulation.

The simulator implementation ``MultithreadedSimulatorImpl`` is a derived
class from the base simulator. It converts calls to the base simulator into
calls to logical processes based on the context of the current thread.
It also provides a partition method for automatic fine-grained topology partition.

For distributed simulation with MPI, we added ``HybridSimulatorImpl`` in the
``mpi`` module. This simulator uses both ``MtpInterface`` and ``MpiInterface``
to coordinate local LPs and global MPI communications. We also modified the
module to make it locally thread-safe.

Running Multithreaded Simulations
*********************************

Prerequisites
+++++++++++++
.. highlight:: bash

For multithreaded simulation on a single machine with many cores, ensure
that your system supports pthread library. For hybrid distributed simulation,
ensure that MPI is installed, as well as mpic++. In Ubuntu repositories,
these are openmpi-bin, openmpi-common, openmpi-doc, libopenmpi-dev. In
Fedora, these are openmpi and openmpi-devel.

Building and running examples
+++++++++++++++++++++++++++++

If you already built |ns3| without MTP enabled, you must re-build::

    $ ./ns3 distclean

Configure |ns3| with the --enable-mtp option::

    $ ./ns3 configure --enable-examples --enable-tests --enable-mtp

Ensure that MTP is enabled by checking the optional features shown from the
output of configure. If you want to use the hybrid simulator, you also have
to pass the --enable-mpi option.

Next, build |ns3|::

    $ ./ns3

After building |ns3| with MTP enabled, the example programs are now
ready to run. Here are a few adapted examples (you can run these original
examples by omiting the ``-mtp`` in the program name)::

    $ ./ns3 run dctcp-example-mtp
    $ ./ns3 run rping-simple-network-mtp
    $ ./ns3 run simple-multicast-flooding-mtp

An example simulating the fat-tree topology with the multithreaded simulator
and the hybrid simulator::

    $ ./ns3 run "fat-tree-mtp --thread=4"
    $ ./ns3 run "fat-tree-mtp --command-template "mpirun -np 2 %s --thread=2"

The thread parameter is the number of threads to use (for each process in the
hybrid case).

Advanced Options
++++++++++++++++
.. highlight:: cpp

These options can be modified at the beginning of the ``main`` function using
the native config syntax of ns-3.

You can also change the default maximum number of threads by setting

    Config::SetDefault("ns3::MultithreadedSimulatorImpl::MaxThreads", UintegerValue(8));
    Config::SetDefault("ns3::HybridSimulatorImpl::MaxThreads", UintegerValue(8));

The automatic partition will cut off stateless links whose delay is above the
threshold. The threshold is automatically calculated based on the delay of every
link. If you are not satisfied with the partition results, you can set a custom
threshold by setting

    Config::SetDefault("ns3::MultithreadedSimulatorImpl::MinLookahead", TimeValue(NanoSeconds(500));
    Config::SetDefault("ns3::HybridSimulatorImpl::MinLookahead", TimeValue(NanoSeconds(500));

The scheduling method determines the priority (estimated completion time of the
next round) of each logical process. There are five available options:

- ``ByExecutionTime``: LPs with a higher execution time of the last round will have higher priority.
- ``ByPendingEventCount``: LPs with more pending events of this round will have higher priority.
- ``ByEventCount``: LPs with more pending events of this round will have higher priority.
- ``BySimulationTime``: LPs with larger current clock time will have higher priority.
- ``None``: Do not schedule. The partition's priority is based on their ID.

Many experiments show that the first one usually leads to better performance.
However, you can still choose one according to your taste by setting

    GlobalValue::Bind("PartitionSchedulingMethod", StringValue("ByExecutionTime"));

By default, the scheduling period is 2 when the number of partitions is less than
16, 3 when it is less than 256, 4 when it is less than 4096, etc. Since more
partitions lead to more scheduling costs. You can also set how frequently scheduling
occurs by setting

    GlobalValue::Bind("PartitionSchedulingPeriod", UintegerValue(4));

Tracing During Multithreaded Simulations
****************************************

Unison resolved a lot of thread-safety issues with ns-3's architecture. You don't
need to consider these issues on your own for most of the time, except if you have
custom global statistics other than the built-in flow-monitor. In the latter case,
if multiple nodes can access your global statistics, you can replace them with
atomic variables via ``std::atomic<>``. When collecting tracing data such as Pcap,
it is strongly recommended to create separate output files for each node instead
of a single trace file. For complex custom data structures, you can create critical
sections by adding

    MtpInterface::CriticalSection cs;

at the beginning of your methods.
