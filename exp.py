#! /usr/bin/python3

import datetime
import itertools
import math
import os
import re
import shutil
import subprocess
import time
from sys import argv


# --- configurations of experiment hosts (feel free to modify these configurations) ---


conf = {
    # number of physical cores per host
    'maxcore': 24,

    # hosts for hybrid simulation (they should be able to log in via ssh without password)
    'hosts': [
        '172.16.0.2',
        '172.16.0.3',
        '172.16.0.4',
        '172.16.0.5',
        '172.16.0.6',
        '172.16.0.7',
    ],

    # callback when the experiment finished
    'report': lambda msg: print(msg)
}


# --- experiment class definition ---


class ResultCallback:
    '''Result callback function wrapper class'''

    def __init__(self, names=[], callback=None):
        '''
        Parameters:
            names (list[str]): Result name that can be obtained by the callback function
            callback (callable): The actual callback function. This function should return a list, each value in the list corresponds to each name in the names
        '''
        self.names = names
        self.callback = callback

    def __call__(self) -> list:
        '''Call the actual callback function. If the function is not provided, simply return an empty list'''
        if callable(self.callback):
            return self.callback()
        else:
            return []

    @staticmethod
    def make(names: list):
        '''
        Make the result callback function wrapper
        
        Parameters:
            names (list[str]): Result name that can be obtained by the callback function
        '''
        def wrapper(func):
            return ResultCallback(names, func)
        return wrapper


class Experiment:
    def __init__(self, name: str):
        ts = datetime.datetime.now().strftime('%m%d-%H%M')
        self.name = name
        self.cmd_output = open(f'results/{name}-{ts}.sh', 'wt', buffering=1)
        self.raw_output = open(f'results/{name}-{ts}.txt', 'wt', buffering=1)
        self.data_output = open(f'results/{name}-{ts}.csv', 'wt', buffering=1)

    def __del__(self):
        self.cmd_output.close()
        self.raw_output.close()
        self.data_output.close()

    def cmd(self, cmd: list, run=True):
        '''
        Log the automated shell commands to .sh and .txt files

        Parameters:
            cmd: The automated commands
            run: Set to False if just to log the command without running it
        '''

        cmd_str = ' '.join([f'"{c}"' if ' ' in c else c for c in cmd]) + '\n'
        self.cmd_output.write(cmd_str)
        self.raw_output.write(cmd_str)
        if run:
            subprocess.run(cmd)

    def run(self, programs: list, simulators: list, template_cmd='', branch='unison-evaluations', callback=ResultCallback(), **kwargs):
        '''
        Run a batch of experiments

        Parameters:
            programs (list): Simulation programs
            simulators (list): Yype of simulators to be used, this can be set to barrier, nullmsg, unison and default
            template_cmd (str): Command wrapper for actually executed commands, used for perf tools
            branch (str): Check to this branch first before running. This is used to 
            callback (ResultCallback): A callback to process the generated text and CSV files during simulation
            kwargs: These are the various parameters or parameter lists for simulation
        '''

        # checkout
        self.cmd(['git', 'checkout', branch])

        # write header
        self.data_output.write('program,simulator')
        for arg in sorted(kwargs.keys()):
            self.data_output.write(',' + arg)
        for name in callback.names:
            self.data_output.write(',' + name)
        self.data_output.write(',ret,flow_count,fflow_count,fct,ffct,e2ed,throughput,nthroughput,ev,t\n')

        # enumerate every possible argument permutations
        arg_permutations = []
        for k, v in kwargs.items():
            if isinstance(v, list):
                arg_permutations.append(itertools.product([k], v))
            else:
                arg_permutations.append([(k, v)])

        # run all programs under every argument combinations
        for args in list(itertools.product(*arg_permutations)):
            for program in programs if isinstance(programs, list) else [programs]:
                for simulator in simulators if isinstance(simulators, list) else [simulators]:
                    self.run_once(program, simulator, template_cmd=template_cmd, callback=callback, **dict([(k, v(dict(args)) if callable(v) else v) for k, v in args]))

        # check back
        self.cmd(['git', 'checkout', 'unison-evaluations'])

        # push finished notifications
        if 'report' in conf:
            conf['report'](f'Experiment {self.name} finished.')

    def run_once(self, program: str, simulator: str, template_cmd='', callback=ResultCallback(), **kwargs):
        '''
        Build and run a single experiment

        Parameters:
            program (str): simulation program
            simulator (str): type of simulator to be used, this can be set to barrier, nullmsg, unison and default
            template_cmd (str): command wrapper for actually executed commands, used for perf tools
            callback (ResultCallback): a callback to process the generated text and CSV files during simulation
            kwargs: these are the various parameters or parameter lists for simulation
        '''

        args = kwargs.copy()
        enable_modules = ['applications', 'flow-monitor', 'mpi', 'mtp', 'nix-vector-routing', 'point-to-point']
        configure_cmd = ['./ns3', 'configure', '-d', 'optimized', '--enable-modules', ','.join(enable_modules)]
        mpi_cmd = ''

        # configure
        core = args.pop('core', 1)
        if simulator == 'barrier':
            args['nullmsg'] = False
            configure_cmd.append('--enable-mpi')
            mpi_cmd = f'mpirun -n {core} --map-by ppr:{conf["maxcore"]}:node --bind-to core'
        elif simulator == 'nullmsg':
            args['nullmsg'] = True
            configure_cmd.append('--enable-mpi')
            mpi_cmd = f'mpirun -n {core} --map-by ppr:{conf["maxcore"]}:node --bind-to core'
        elif simulator == 'unison' and core <= conf['maxcore']:
            args['thread'] = core
            configure_cmd.append('--enable-mtp')
        elif simulator == 'unison' and core > conf['maxcore']:
            args['thread'] = math.ceil(core / math.ceil(core / conf["maxcore"]))
            configure_cmd.append('--enable-mtp')
            configure_cmd.append('--enable-mpi')
            mpi_cmd = f'mpirun -n {math.ceil(core / conf["maxcore"])} --map-by ppr:1:node --bind-to none'
        elif simulator != 'default':
            print(f'No such simulator: {simulator}, skipping')
            return
        if mpi_cmd != '' and core > conf['maxcore']:
            mpi_cmd += f' --host ' + ','.join([f'{h}:{conf["maxcore"]}' for h in conf['hosts']])
        template_cmd = f'{mpi_cmd} {template_cmd}'.strip()

        # build
        self.cmd(['./ns3', 'clean'])
        self.cmd(configure_cmd)
        self.cmd(['./ns3', 'build', program])

        # run
        args_str = ' '.join([f'--{k}={v}' for k, v in args.items()])
        if template_cmd == '':
            run_cmd = ['./ns3', 'run', f'{program} {args_str}'.strip()]
        else:
            run_cmd = ['./ns3', 'run', program, '--command-template', f'{template_cmd} %s {args_str}'.strip()]
        self.cmd(run_cmd, run=False)
        process = subprocess.Popen(run_cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        # get results from the output of the simulation process
        ev = 0
        t = 0
        flow_count = 0
        fflow_count = 0
        fct = 0
        ffct = 0
        e2ed = 0
        throughput = 0
        nthroughput = 0

        t_start = time.time()
        for line in process.stdout:
            m = re.match(r'  Detected #flow = (.*)\n', line)
            if m is not None:
                flow_count = m.group(1)
            m = re.match(r'  Finished #flow = (.*)\n', line)
            if m is not None:
                fflow_count = m.group(1)
            m = re.match(r'  Average FCT \(all\) = (.*)us\n', line)
            if m is not None:
                fct = m.group(1)
            m = re.match(r'  Average FCT \(finished\) = (.*)us\n', line)
            if m is not None:
                ffct = m.group(1)
            m = re.match(r'  Average end to end delay = (.*)us\n', line)
            if m is not None:
                e2ed = m.group(1)
            m = re.match(r'  Average flow throughput = (.*)Gbps\n', line)
            if m is not None:
                throughput = m.group(1)
            m = re.match(r'  Network throughput = (.*)Gbps\n', line)
            if m is not None:
                nthroughput = m.group(1)
            m = re.match(r'  Event count = (.*)\n', line)
            if m is not None:
                ev = m.group(1)
            m = re.match(r'  Simulation time = (.*)s\n', line)
            if m is not None:
                t = m.group(1)
            self.raw_output.write(line)
        process.wait()
        t_end = time.time()
        if t == 0:
            t = t_end - t_start

        # output statistics to the CSV file
        self.data_output.write(f'{program},{simulator}')
        for _, value in sorted(kwargs.items()):
            self.data_output.write(f',{value}')
        for value in callback():
            self.data_output.write(f',{value}')
        self.data_output.write(f',{process.returncode},{flow_count},{fflow_count},{fct},{ffct},{e2ed},{throughput},{nthroughput},{ev},{t}\n')
        self.data_output.flush()

        # push finished notifications
        if 'report' in conf:
            conf['report'](f'{self.name}: {program}-{simulator} returns {process.returncode} in {t} seconds.\n\n{run_cmd}')


# --- experiment result callbacks ---


@ResultCallback.make(['msg', 'sync', 'exec'])
def get_mpi_time() -> list:
    '''This callback reads msg, sync, exec time (i.e., P, S, M) of each LP for the barrier and nullmsg simulator, output P and S ratio in each round, and calcluate the average of the sum of every LP's P, S and M in every rounds'''
    t_msg = 0
    t_sync = 0
    t_exec = 0
    t_sync_list = []
    t_exec_list = []
    system_count = 0
    # calculate average time
    for filename in os.listdir('results'):
        if filename.startswith('BS-') or filename.startswith('NM-'):
            t_sync_round_list = []
            t_exec_round_list = []
            system_count += 1
            fullname = f'results/{filename}'
            with open(fullname, 'rt') as f:
                f.readline()
                line = f.readline().split(',')
                t_msg += int(line[-3])
                t_sync += int(line[-2])
                t_exec += int(line[-1])
                for line in f.readlines():
                    line = line.split(',')
                    t_sync_round_list.append(int(line[-2]))
                    t_exec_round_list.append(int(line[-1]))
            t_sync_list.append(t_sync_round_list)
            t_exec_list.append(t_exec_round_list)
            os.unlink(fullname)
    # convert to seconds
    t_msg /= system_count * 1e9
    t_sync /= system_count * 1e9
    t_exec /= system_count * 1e9
    # output exec time in each round
    with open(f'results/mpi-exec-{system_count}.csv', 'wt') as f:
        f.write('rank,round,exec\n')
        for rank in range(system_count):
            for rnd in range(101):
                try:
                    f.write(f'{rank},{rnd},{sum(t_exec_list[rank][rnd * 100:rnd * 100 + 100])}\n')
                except IndexError:
                    f.write(f'{rank},{rnd},-1\n')
            f.write('\n')
    # output sync ratio in each round
    with open(f'results/mpi-ratio-{system_count}.csv', 'wt') as f:
        f.write('round,ratio\n')
        for rnd in range(len(t_exec_list[0])):
            f.write(f'{rnd},{t_sync_list[rank][rnd] / (t_sync_list[rank][rnd] + t_exec_list[rank][rnd])}\n')
    return [t_msg, t_sync, t_exec]


@ResultCallback.make(['msg', 'sync', 'exec', 'sorting', 'process', 'slowdown'])
def get_mtp_time() -> list:
    '''This callback reads msg, sync, exec time (i.e., P, S, M) and the slowdown factor of each thread for Unison, output P and S in each round, and calcluate the average of the sum of every thread's P, S and M in every rounds'''
    t_sync = 0
    t_exec = 0
    t_sync_list = []
    t_exec_list = []
    system_count = 0
    # calculate average time
    for filename in os.listdir('results'):
        if filename.startswith('MT-'):
            t_sync_round_list = []
            t_exec_round_list = []
            system_count += 1
            fullname = f'results/{filename}'
            with open(fullname, 'rt') as f:
                f.readline()
                line = f.readline().split(',')
                t_sync += int(line[-2])
                t_exec += int(line[-1])
                for line in f.readlines():
                    line = line.split(',')
                    t_sync_round_list.append(int(line[-2]))
                    t_exec_round_list.append(int(line[-1]))
            t_sync_list.append(t_sync_round_list)
            t_exec_list.append(t_exec_round_list)
            os.unlink(fullname)
    # get total time
    with open('results/MT.csv') as f:
        line = f.readlines()[-1].split(',')
        t_msg = int(line[-4])
        t_sorting = int(line[-3])
        t_process = int(line[-2])
        slowdown = float(line[-1])
        os.unlink('results/MT.csv')
    # convert to seconds
    t_msg /= system_count * 1e9
    t_sync /= system_count * 1e9
    t_exec /= system_count * 1e9
    t_sorting /= 1e9
    t_process /= 1e9
    # output exec time in each round
    with open(f'results/mtp-exec-{system_count}.csv', 'wt') as f:
        f.write('rank,round,exec\n')
        for rank in range(system_count):
            for rnd in range(101):
                try:
                    f.write(f'{rank},{rnd},{sum(t_sync_list[rank][rnd * 100:rnd * 100 + 100])}\n')
                except IndexError:
                    f.write(f'{rank},{rnd},-1\n')
            f.write('\n')
    # output sync ratio in each round
    with open(f'results/mtp-ratio-{system_count}.csv', 'wt') as f:
        f.write('round,ratio\n')
        for rnd in range(len(t_exec_list[0])):
            f.write(f'{rnd},{t_sync_list[rank][rnd] / (t_sync_list[rank][rnd] + t_exec_list[rank][rnd])}\n')
    return [t_msg, t_sync, t_exec, t_sorting, t_process, slowdown]


@ResultCallback.make(['miss'])
def get_cache_miss() -> list:
    '''This callback reads cache misses from the linux perf output'''
    cache_miss = -1
    with open('results/perf.txt') as f:
        for line in f.readlines():
            if 'cache-misses' in line:
                cache_miss = int(line.strip().removesuffix('cache-misses').strip().replace(',', ''))
    os.unlink('results/perf.txt')
    return [cache_miss]


# --- experiment parameters ---


if __name__ == '__main__':
    # clean up some intermeidate results to avoid error
    subprocess.run('rm -f results/BS*', shell=True)
    subprocess.run('rm -f results/NM*', shell=True)
    subprocess.run('rm -f results/MT*', shell=True)

    if len(argv) != 2:
        print('Usage:')
        print('./exp.py init')
        print('./exp.py update')
        print('./exp.py [experiment name]')
        exit(0)

    # initialize environments
    if argv[1] == 'init':
        subprocess.run('sudo apt update', shell=True)
        # install dependencies
        subprocess.run('sudo apt install build-essential cmake git openmpi-bin openmpi-doc libopenmpi-dev linux-tools-generic ninja-build nfs-common texlive-latex-recommended texlive-fonts-extra -y', shell=True)
        # allow perf
        subprocess.run('sudo sysctl -w kernel.perf_event_paranoid=-1', shell=True)
        # disable hyper-threading
        subprocess.run('echo off | sudo tee /sys/devices/system/cpu/smt/control', shell=True)

    elif argv[1] == 'update':
        subprocess.run('git fetch origin', shell=True)
        subprocess.run('git checkout unison-evaluations', shell=True)
        subprocess.run('git reset --hard ns-3.36.1', shell=True)
        subprocess.run('git rebase origin/unison-evaluations', shell=True)
        subprocess.run('git checkout unison-evaluations-for-mpi', shell=True)
        subprocess.run('git reset --hard ns-3.36.1', shell=True)
        subprocess.run('git rebase origin/unison-evaluations-for-mpi', shell=True)
        subprocess.run('git checkout unison-evaluations-for-mtp', shell=True)
        subprocess.run('git reset --hard ns-3.36.1', shell=True)
        subprocess.run('git rebase origin/unison-evaluations-for-mtp', shell=True)
        subprocess.run('git checkout unison-evaluations', shell=True)

    # 1 (7d)
    elif argv[1] == 'fat-tree-distributed':
        e = Experiment(argv[1])
        e.run('fat-tree', ['barrier', 'nullmsg', 'unison'],
              k=8,
              cluster=[48, 72, 96, 120, 144],
              # cluster=8, # for small-scale
              delay=3000,
              bandwidth='100Gbps',
              flow=False,
              incast=1,
              victim=lambda args: '-'.join([str(i) for i in range(int(args['k']) ** 2 // 4)]),
              time=0.1,
              interval=0.01,
              flowmon=False,
              core=lambda args: args['cluster'])

    # 1 (4d)
    elif argv[1] == 'fat-tree-default':
        e = Experiment(argv[1])
        e.run('fat-tree', 'default',
              k=8,
              cluster=[48, 72, 96],
              # cluster=8, # for small-scale
              delay=3000,
              bandwidth='100Gbps',
              flow=False,
              incast=1,
              victim=lambda args: '-'.join([str(i) for i in range(int(args['k']) ** 2 // 4)]),
              time=0.1,
              interval=0.01,
              flowmon=False,
              core=1)

    # 5a (18h)
    elif argv[1] == 'mpi-sync-incast':
        e = Experiment(argv[1])
        e.run('fat-tree', ['nullmsg', 'barrier'],
              branch='unison-evaluations-for-mpi',
              callback=get_mpi_time,
              k=8,
              delay=3000,
              bandwidth='100Gbps',
              flow=False,
              incast=[0, 0.2, 0.4, 0.6, 0.8, 1],
              victim=lambda args: '-'.join([str(i) for i in range(int(args['k']) ** 2 // 4)]),
              time=0.1,
              interval=0.01,
              flowmon=True,
              core=8)

    # 5b (1h)
    elif argv[1] == 'mpi-sync':
        e = Experiment(argv[1])
        e.run('fat-tree', 'barrier',
              branch='unison-evaluations-for-mpi',
              callback=get_mpi_time,
              k=8,
              delay=3000,
              bandwidth='100Gbps',
              flow=False,
              time=0.1,
              interval=0.01,
              flowmon=True,
              core=8)
        shutil.move('results/mpi-ratio-8.csv', 'results/mpi-sync.csv')
        shutil.move('results/mpi-exec-8.csv', 'results/mpi-exec.csv')

    # 5c (20min)
    elif argv[1] == 'mpi-sync-delay':
        e = Experiment(argv[1])
        e.run('fat-tree', ['barrier', 'nullmsg'],
              branch='unison-evaluations-for-mpi',
              callback=get_mpi_time,
              k=8,
              delay=[3000000, 300000, 30000, 3000, 300],
              bandwidth='10Gbps',
              flow=False,
              time=0.1,
              interval=0.01,
              flowmon=True,
              core=8)

    # 5d (10min)
    elif argv[1] == 'mpi-sync-bandwidth':
        e = Experiment(argv[1])
        e.run('fat-tree', ['barrier', 'nullmsg'],
              branch='unison-evaluations-for-mpi',
              callback=get_mpi_time,
              k=8,
              delay=30000,
              bandwidth=['2Gbps', '4Gbps', '6Gbps', '8Gbps', '10Gbps'],
              flow=False,
              load=lambda args: 1 / int(args['bandwidth'].removesuffix('Gbps')),
              time=0.1,
              interval=0.01,
              flowmon=True,
              core=8)

    # 8 (2h)
    elif argv[1] == 'dqn':
        e = Experiment(argv[1])
        e.run('dqn', ['barrier', 'nullmsg', 'unison', 'default'],
              k=[4, 8],
              cluster=[4, 8],
              delay=500000,
              bandwidth='100Mbps',
              tcp='ns3::TcpCubic',
              load=0.7,
              time=20,
              interval=1,
              flowmon=True,
              core=16)

    # 9 (1d)
    elif argv[1] == 'flexible':
        e = Experiment(argv[1])
        e.run('fat-tree', 'unison',
              k=8,
              # k=4, # for small-scale
              delay=3000,
              bandwidth='100Gbps',
              core=[24, 20, 16, 12, 8, 4, 2])
        # core=[8, 6, 4, 2]) # for small-scale

    # 9 (3d)
    elif argv[1] == 'flexible-barrier':
        e = Experiment(argv[1])
        e.run('fat-tree', 'barrier',
              k=8,
              # k=4, # for small-scale
              delay=3000,
              bandwidth='100Gbps',
              core=[8, 4, 2])
        # core=[4, 2]) # for small-scale

    # 9 (1d)
    elif argv[1] == 'flexible-default':
        e = Experiment(argv[1])
        e.run('fat-tree', 'default',
              k=8,
              # k=4, # for small-scale
              delay=3000,
              bandwidth='100Gbps')

    # 10a (3h)
    elif argv[1] == 'mtp-sync-incast':
        e = Experiment(argv[1])
        e.run('fat-tree', 'unison',
              branch='unison-evaluations-for-mtp',
              callback=get_mtp_time,
              k=8,
              delay=3000,
              bandwidth='100Gbps',
              flow=False,
              incast=[0, 0.2, 0.4, 0.6, 0.8, 1],
              victim=lambda args: '-'.join([str(i) for i in range(int(args['k']) ** 2 // 4)]),
              time=0.1,
              interval=0.01,
              flowmon=True,
              core=8)

    # 10b (40min)
    elif argv[1] == 'mtp-sync':
        e = Experiment(argv[1])
        e.run('fat-tree', 'unison',
              branch='unison-evaluations-for-mtp',
              callback=get_mtp_time,
              k=8,
              delay=3000,
              bandwidth='100Gbps',
              flow=False,
              time=0.1,
              interval=0.01,
              flowmon=True,
              core=8)
        shutil.move('results/mtp-ratio-8.csv', 'results/mtp-sync.csv')
        shutil.move('results/mtp-exec-8.csv', 'results/mtp-exec.csv')

    # 11a (4d)
    elif argv[1] == 'torus-distributed':
        e = Experiment(argv[1])
        e.run('torus', ['barrier', 'nullmsg', 'unison'],
              row=48,
              col=48,
              delay=30000,
              bandwidth='10Gbps',
              incast=0.5,
              time=1,
              core=[144, 96, 72, 48])

    # 11a (5d)
    elif argv[1] == 'torus':
        e = Experiment(argv[1])
        e.run('torus', ['barrier', 'nullmsg', 'unison'],
              row=48,
              col=48,
              delay=30000,
              bandwidth='10Gbps',
              incast=0.5,
              time=1,
              core=[24, 12, 6])

    # 11b (40min)
    elif argv[1] == 'bcube':
        e = Experiment(argv[1])
        e.run('bcube', 'unison',
              n=8,
              delay=3000,
              bandwidth='10Gbps',
              cdf=['scratch/cdf/google-rpc.txt', 'scratch/cdf/web-search.txt'],
              incast=0.5,
              victim=lambda args: '-'.join([str(i) for i in range(int(args['n']))]),
              time=0.1,
              interval=0.01,
              core=[8, 16])

    # 11b (2h)
    elif argv[1] == 'bcube-old':
        e = Experiment(argv[1])
        e.run('bcube', ['barrier', 'nullmsg'],
              n=8,
              delay=3000,
              bandwidth='10Gbps',
              cdf=['scratch/cdf/google-rpc.txt', 'scratch/cdf/web-search.txt'],
              incast=0.5,
              victim=lambda args: '-'.join([str(i) for i in range(int(args['n']))]),
              time=0.1,
              interval=0.01,
              core=8)

    # 11b (1d)
    elif argv[1] == 'bcube-default':
        e = Experiment(argv[1])
        e.run('bcube', 'default',
              n=8,
              delay=3000,
              bandwidth='10Gbps',
              cdf=['scratch/cdf/google-rpc.txt', 'scratch/cdf/web-search.txt'],
              incast=0.5,
              victim=lambda args: '-'.join([str(i) for i in range(int(args['n']))]),
              time=0.1,
              interval=0.01)

    # 11c (3d)
    elif argv[1] == 'wan':
        e = Experiment(argv[1])
        e.run('wan', ['unison', 'default'],
              topo=['scratch/topos/geant.graphml', 'scratch/topos/chinanet.graphml'],
              delay=5000000,
              bandwidth='10Gbps',
              ecn=False,
              rip=True,
              tcp='ns3::TcpBbr',
              load=0.5,
              time=10,
              interval=1,
              core=16)

    # 12 (30min)
    elif argv[1] == 'accuracy':
        e = Experiment(argv[1])
        e.run('fat-tree', ['barrier', 'nullmsg', 'unison', 'default'],
              k=4,
              cluster=4,
              delay=500000,
              bandwidth='100Mbps',
              buffer='100p',
              ecn=False,
              flow=True,
              tcp='ns3::TcpNewReno',
              size=500,
              load=0.7,
              incast=0.5,
              victim=lambda args: '-'.join([str(int(args['k']) ** 2 * int(args['cluster']) // 4 - i - 1) for i in range(int(args['k']))]),
              time=20,
              interval=1,
              flowmon=True,
              core=4)

    # 13 (4h)
    elif argv[1] == 'deterministic':
        e = Experiment(argv[1])
        e.run('fat-tree', ['barrier', 'nullmsg', 'unison'],
              k=8,
              delay=500000,
              bandwidth='1Gbps',
              seed='GREAT-NASA',
              tcp='ns3::TcpCubic',
              load=0.7,
              flowmon=True,
              core=[8] * 20)

    # 14a (1d)
    elif argv[1] == 'partition-cache':
        e = Experiment(argv[1])
        e.run('torus', 'unison',
              template_cmd='perf stat -e cache-misses -o results/perf.txt',
              callback=get_cache_miss,
              row=12,
              col=12,
              delay=30000,
              incast=0.5,
              system=[144, 72, 48, 36, 24, 18, 12, 6, 4, 3, 2, 1],
              time=1,
              interval=0.1,
              core=1)

    # 14b (4h)
    elif argv[1] == 'scheduling-metrics':
        e = Experiment(argv[1])
        e.run('fat-tree', 'unison',
              branch='unison-evaluations-for-mtp',
              callback=get_mtp_time,
              k=8,
              bandwidth='100Gbps',
              time=0.1,
              sort=['None', 'ByExecutionTime', 'ByPendingEventCount'],
              core=[4, 8, 12, 16])

    # 14c (2h)
    elif argv[1] == 'scheduling-period':
        e = Experiment(argv[1])
        e.run('fat-tree', 'unison',
              branch='unison-evaluations-for-mtp',
              callback=get_mtp_time,
              k=8,
              bandwidth='100Gbps',
              time=0.1,
              period=[1, 2, 4, 8, 16, 32, 64, 128, 256, 512],
              core=16)

    else:
        print('No such experiment!')

    # clean up some intermeidate results to avoid confusing
    subprocess.run('rm -f results/mpi-ratio-*', shell=True)
    subprocess.run('rm -f results/mtp-ratio-*', shell=True)
    subprocess.run('rm -f results/mpi-exec-*', shell=True)
    subprocess.run('rm -f results/mtp-exec-*', shell=True)
