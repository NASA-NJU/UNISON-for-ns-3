
Artifact Evaluation for UNISON
================================
A fast and user-transparent parallel simulator implementation for ns-3.

## Artifact Evaluation Overview

In this branch, you can find various topology models in the `scratch` folder.

There are a lot of parameters you can set via command lines for each topology model.
The parameters are stored and configured in `scratch/utils/common.cc`.
Besides, we write a Poisson traffic generator `scratch/utils/traffic-generator.cc` to generate workload according to a flow distribution.
Every topology model will output flow statistics after the simulation if the FlowMonitor is enabled.

In `scratch/utils/common.cc`, we defined a lot of macros so that the simulator for these models is automatically set according to your build profile.
`--enable-mtp` will use UNISON, `--enable-mpi` will use traditional distributed simulators.
If both are enabled, the hybrid simulator is used, and the default sequential simulator is used if neither is enabled.

To compare these simulators and parameters, we provided a utility script `exp.py`.
It can initialize required environments, install dependencies and run simulations according to your parameter sets automatically.
Please see the document below and the source code for more details.

## Hardware Requirements

In order to run all experiments of UNISON, you should have at least 144 CPU cores and 512GB of memory.
These computation resources can come from either a single host with many CPU cores (e.g., ARM or AMD Threadripper) or multiple identically configured hosts within a LAN cluster.

However, most of the experiments can be performed on a single commodity computing server with at least 24 CPU cores and 128GB of memory. Moreover, about half of the experiments can be performed just on a single commodity PC with at least 8 CPU cores and 16GB of memory.

If your current hardware doesn't meet the very first requirements (144 cores, 256GB of memory), we provide some alternative small-scale experiments that can be run on a single commodity PC (8 cores, 16GB of memory) or a single commodity computing server (24 cores, 128GB of memory) and reflect similar phenomena at a large scale.

You can see the hardware requirements of each experiment [here](#requirements-of-each-experiment) or by checking the `core` argument in `exp.py`.

## Software Dependencies

A Unix-like operating system installed with
- Python 3.8 or above
- g++-7, clang-10 or Xcode 11
- Git
- CMake
- Linux `perf`
- OpenMPI
- NFS server/client

is required to compile and run UNISON and all of its related experiments.
To plot the figures presented in the paper from the experiment data locally, a LaTeX installation with the PGFPlots package is required.
We recommend to run our experiments with Ubuntu 22.04.

## Setup

If you are using Ubuntu, you can initialize the environment in every host you want to run by executing

```shell
./exp.py init
```

This will turn off hyperthreading, allow kernel profiling and install software dependencies including CMake, OpenMPI, TeX Live and Linux perf tools.

Additionally, if you want to run distributed simulation experiments (whose name typically contains `-distributed` in `exp.py`), you should have multiple hosts with the same configuration in a LAN.
You should choose one of the hosts as the master host.
The master host should be able to SSH into other hosts directly without passwords.
Then, you should adjust the `conf` variable at the top of `exp.py` of the master host.
Please make sure that the maximum number of physical cores per host is set according to your hardware info and that the IP addresses of every host (including the master host) are set correctly.

To make sure every host can see the same code and executables, you have to set up an NFS service on the master host, assuming you cloned this repository in your home folder:

```shell
sudo apt install nfs-kernel-server
echo "/home/$(whoami)/UNISON-for-ns-3 *(rw,sync,no_root_squash,no_subtree_check)" | sudo tee /etc/exports
sudo exportfs -a
sudo service nfs-kernel-server restart
```

Then, mount NFS on every other host:

```shell
mkdir UNISON-for-ns-3
sudo mount -t nfs $IP_OF_YOUR_MASTER_HOST:/home/$(whoami)/UNISON-for-ns-3 ~/UNISON-for-ns-3
```

After these steps, you can now safely run distributed simulation experiments by invoking `exp.py` on the master host.

## Running Experiments

To run one experiment, you can type
```shell
nohup ./exp.py $EXPERIMENT_NAME > nohup.out 2>&1 &
```

and the script will handle the compiling, running, result parsing process automatically for you.
It is recommended to use `nohup` since the experiment can take hours to days to finish.
You can also use `tmux` or `screen` instead of `nohup`.

You can see all available experiment names, their parameters and their estimated finish time at the bottom of the `exp.py`.
The description of each parameter and their default values are in the `scratch/utils/common.cc`.
Here we list some common parameters used during our experiments:

- `k`: Fat-tree degree
- `cluster`: # of clusters in a fat-tree, the default is set to `k`
- `row`: # of 2D-Torus row
- `col`: # of 2D-Torus column
- `delay`: Link propagation delay in nanoseconds
- `bandwidth`: Link bandwidth
- `buffer`: Buffer size for each port, default is 4MB
- `ecn`: Enable ECN, default is true
- `ecmp`: Use per-packet ECMP routing, default is true
- `flow`: Use per-flow ECMP routing, default is false
- `rip`: Use RIP dynamic routing, default is false
- `tcp`: The TCP protocol to be used, default is DCTCP
- `cdf`: Flow size distribution file. You can find these CDF files in `scratch/cdf`
- `load`: Traffic load relative to the bisection bandwidth of the current topology
- `incast`: Incast traffic ratio
- `victim`: Incast traffic destinations. Multiple destinations are separated by comma
- `seed`: Random seed for flow generation and other random events in the simulation
- `flowmon`: Use FlowMonitor to get flow statistics, default is false
- `time`: Simulated time in seconds
- `interval`: Print the simulation progress for every `interval` seconds
- `core`: Number of threads for UNISON, or number of LPs for other PDES algorithms

If you want to iteratively run experiments under multiple parameter combinations, you can pass a list instead of a single value.
Moreover, if your parameters are co-relative, you can pass lambda functions to set your parameters.
The `exp.py` script will automatically handle these cases for you.
Feel free to adjust these parameters and create new experiments for your own research.

## Processing Experiment Data

The raw experiment data and logs are stored in the `results` folder.
Each experiment will generate a `.sh` file, a `.txt` file and a `.csv` file, all named with the format `NAME-DATE-TIME`.

The first one is all the shell commands automated by `exp.py`.
If you want to compile and run the experiment manually, you can see the output of the `.sh` file.

The second one is all the text output of the experiment program, including how the topology is set, how the flows are generated and how the IP addresses are assigned.

The last one contains the raw experiment data.
Each line in the `.csv` file represents an experiment record.
To parse the results, we provide a utility script `process.py` to convert these raw experiment data to CSV files suitable for plotting.
You can use this script by giving it a figure ID:

```shell
./process.py $FIGURE_ID
```

Then the processed CSV file will be saved in the `results` folder and the figure ID is the filename.
This utility script will pick up the latest experiment results if you have run one of the experiments multiple times.
You can see the code for more details.

After processing, you can get the plot by compiling `results/plot.tex` with `pdflatex`, which will produce the final plot in `results/plot.pdf`.
This TeX file checks whether the CSV file to be plotted exists and plots the figure via the PGFPlot package.

## Mapping Claims to Figures

| Claim ID | Claim    | Figure IDs |
|----------|-----------|------------|
|1| UNISON can achieve 10x speedup over existing PDES approaches | 1 |
|2| The synchronization time of existing PDES approaches gradually dominates as the traffic inhomogeneity increases | 5a |
|3| The synchronization time ratio is high in a transient time window for existing PDES approaches, even if the traffic pattern is balanced in macro | 5b |
|4| The synchronization time is long for low-latency and high-bandwidth networks for existing PDES approaches | 5c, 5d |
|5| UNISON can significantly reduce the synchronization time to near-zero | 9a, 9b |
|6| UNISON exhibit super-linear speedup and its parallelism is flexible to set | 8b |
|7*| UNISON is also fast with other topologies and under different traffic patterns | 10b |
|8| The output of UNISON is deterministic under multiple runs while other PDES approaches are not | 11a, 11b |
|9| Fine-grained partition of UNISON can reduce cache misses which can further reduce the simulation time | 12a |
|10*| The default scheduling metric of UNISON performs better than others and without scheduling | 12c |

## Mapping Figures to Experiment Names

| Figure/Table ID | Required Experiment Names              |
|-----------|----------------------------------------|
| 1         | fat-tree-distributed, fat-tree-default |
| 5a        | mpi-sync-incast                        |
| 5b        | mpi-sync                               |
| 5c        | mpi-sync-delay                         |
| 5d        | mpi-sync-bandwidth                     |
| 8a*       | dqn                                    |
| 8b        | flexible, flexible-barrier, flexible-default |
| 9a        | mtp-sync-incast, mpi-sync-incast       |
| 9b        | mtp-sync                               |
| 10a       | tous-disribured, torus                 |
| 10b       | bcube, bcube-old, bcube-default        |
| 10c       | wan                                    |
| 10d       | rdcn                                   |
| 2*        | accuracy                               |
| 11a       | deterministic                          |
| 11b       | deterministic                          |
| 12a       | partition-cache                        |
| 12b       | partition-corner-case                  |
| 12c       | scheduling-metrics                     |
| 12d       | scheduling-period                      |

- Figure 8 and Table 2 are not covered by your artifact, because they require expensive GPUs and modifications to [MimicNet](https://github.com/eniac/MimicNet) and [DeepQeueuNet](https://github.com/HUAWEI-Theory-Lab/deepqueuenet). However, you can still get part of the data by running the according experiments.

It is notable that multiple experiments required by the same figure should be performed under the same hardware configuration.
If different figures require the same experiment, you can perform this experiment just once.

## Requirements of Each Experiment

| Experiment Name | Cores Required | Dependencies | Expected Machine Time |
|-----------------|----------------|------------------|-----------------------|
| fat-tree-distributed | **144** | OpenMPI, NFS server/client | **7 days** |
| fat-tree-default | 1 | | **4 days** |
| mpi-sync-incast | 8 | OpenMPI | 18 hours |
| mpi-sync | 8 | OpenMPI | 1 hour |
| mpi-sync-delay | 8 | OpenMPI | 20 minutes |
| mpi-sync-bandwidth | 8 | OpenMPI | 10 minutes |
| dqn | **16** | OpenMPI | 2 hours |
| flexible | **24** | | **1 day** |
| flexible-barrier | 8 | OpenMPI | **3 days** |
| flexible-default | 1 | | **1 day** |
| mtp-sync-incast | 8 | | 3 hours |
| mtp-sync | 8 | | 40 minutes |
| torus-distributed | **144** | OpenMPI, NFS server/client | **4 days** |
| torus | **24** | OpenMPI | **5 days** |
| bcube | **16** | | 40 minutes |
| bcube-old | 8 | OpenMPI | 2 hours |
| bcube-default | 1 | | **1 day** |
| wan | **16** | | **3 days** |
| rdcn | 4 | | 3 hours |
| accuracy | **4** | OpenMPI | 30 minutes |
| deterministic | 8 | OpenMPI | 4 hours |
| partition-cache | 1 | Linux perf | **1 day** |
| partition-corner-case | 4 | Linux perf | 20 minutes |
| scheduling-metrics | **16** | | 4 hours |
| scheduling-period | **16** | | **1 day** |

The "Dependencies" field is the required software dependencies in addition to
- Python 3.8 or above
- g++-7, clang-10, or Xcode 11
- Git
- CMake

## Updating Evaluation Branches

These evaluation branches `unison-evaluations`, `unison-evaluations-for-mpi`, and `unison-evaluations-for-mtp` might be rebased since there might be new commits upon their base `unison` branch for bug fixing.
If you want to update these branches, please first backup all your custom modifications on these branches and type the following commands:

```shell
./exp.py update
```

or you can do it manually with Git:

```shell
git fetch origin

git checkout unison-evaluations
git reset --hard ns-3.36.1
git rebase origin/unison-evaluations

git checkout unison-evaluations-for-mpi
git reset --hard ns-3.36.1
git rebase origin/unison-evaluations-for-mpi

git checkout unison-evaluations-for-mtp
git reset --hard ns-3.36.1
git rebase origin/unison-evaluations-for-mtp

git checkout unison-evaluations
```
