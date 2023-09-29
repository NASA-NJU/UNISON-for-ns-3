
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

## Setup

If you are using Ubuntu, you can initialize the environment in every host you want to run by executing

```shell
./exp.py init
```

This will turn off hyperthreading, allow kernel profiling and install dependencies including CMake, MPI and Linux perf tools.

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
nohup ./exp.py $EXPERIMENT_NAME > nohup.out &
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

If you want to see the ongoing experiment, you can use `pgrep` command:
```shell
pgrep -af exp.py
```

and terminate an ongoing experiment via `pkill`:
```shell
pkill -f exp.py
pkill -f ns-3
```

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

After processing, you can get the plot by compiling `results/plot.tex`, which will produce the final plot in `results/plot.pdf`.
This TeX file checks whether the CSV file to be plotted is exist and plots the figure via the PGFPlot package.

## Updating Evaluation Branches

These evaluation branches `unison-evaluations`, `unison-evaluations-for-mpi` and `unison-evaluations-for-mtp` might be rebased since there might be new commits upon their base `unison` branch for bug fixing.
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
