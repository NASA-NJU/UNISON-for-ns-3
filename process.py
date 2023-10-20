#! /usr/bin/python3

from sys import argv
from collections import defaultdict

import os
import re
import shutil
import subprocess


def fullpath(path: str):
    '''Returns the full path given the path relative to this file'''

    return os.path.join(os.path.dirname(__file__), path)


def get_the_latest_result_file(exp_name: str) -> str:
    '''Returns the latest CSV file name given the experiment name'''

    potential_filenames = []
    for filename in os.listdir(fullpath('results')):
        if re.match(exp_name + r'-\d{4}-\d{4}.csv', filename):
            potential_filenames.append(os.path.join('results', filename))
    if len(potential_filenames) == 0:
        print(f'Error: {exp_name}-*.csv not found. Please first run this experiment via:\n'
              f'       ./exp.py {exp_name}')
        exit(1)
    return max(potential_filenames)


def generate_csv_for_pgfplot(figure_name: str, exp: list, x, y, y_post={}, legend=None):
    '''
    Parse raw CSV files given the experiment name and generate a CSV file for plotting

    Parameters:
        figure_name (str): The output figure name
        exp (list): The required experiments by the output figure
        x (str | list[str] | callable): x axis of the figure
        y (str | list[str] | dict{str: callable}): y axis of the figure
        y_post (dict{str: callable}): y axis of the figure, whose values require post calculation
        legend (str | list[str] | None): Legend of the figure
    '''

    data = defaultdict(dict)
    # read raw exp results and store them into `data` by x_value
    for exp_name in exp if isinstance(exp, list) else [exp]:
        with open(fullpath(get_the_latest_result_file(exp_name)), 'rt') as f:
            print('Reading data from experiment', exp_name)
            idx = {key: i for i, key in enumerate(f.readline().strip().split(','))}
            for line in f.readlines():
                line = line.strip().split(',')
                # get x
                if isinstance(x, list):
                    x_label = '-'.join(x)
                    x_value = '-'.join([line[idx[i]] if i in idx else '' for i in x])
                elif callable(x):
                    x_label = 'x'
                    x_value = x(line, idx)
                else:
                    x_label = x
                    x_value = line[idx[x]] if x in idx else ''
                # get legend
                if isinstance(legend, list):
                    legend_label = '-' + '-'.join([line[idx[i]] if i in idx else '' for i in legend])
                elif legend is not None:
                    legend_label = '-' + line[idx[legend]]
                else:
                    legend_label = ''
                # get y
                if isinstance(y, list):
                    for y_name in y:
                        y_label = f'{y_name}{legend_label}'
                        y_value = line[idx[y_name]] if y_name in idx and idx[y_name] < len(line) else ''
                        data[x_value][y_label] = y_value
                        print('  At', x_label, '=', x_value, '=>', y_label, '=', y_value)
                elif isinstance(y, dict):
                    for y_name, y_value_call in y.items():
                        y_label = f'{y_name}{legend_label}'
                        y_value = y_value_call(line, idx)
                        data[x_value][y_label] = y_value
                        print('  At', x_label, '=', x_value, '=>', y_label, '=', y_value)
                else:
                    y_name = y
                    y_label = f'{y_name}{legend_label}'
                    y_value = line[idx[y_name]] if y_name in idx and idx[y_name] < len(line) else ''
                    data[x_value][y_label] = y_value
                    print('  At', x_label, '=', x_value, '=>', y_label, '=', y_value)
    # post calculation
    for y_label, y_value_call in y_post.items():
        print('Post calcluating', y_label)
        for x_value in data:
            y_value = y_value_call(data, x_value)
            data[x_value][y_label] = y_value
            print('  At', x_label, '=', x_value, '=>', y_label, '=', y_value)
    # write to csv
    with open(fullpath(os.path.join('results', figure_name + '.csv')), 'wt') as f:
        y_labels = sorted({y_label for x_value in data for y_label in data[x_value].keys()})
        f.write(f'{x_label},{",".join(y_labels)}\n')
        try:
            sorted_data = sorted(data, key=float)
        except ValueError:
            sorted_data = sorted(data)
        for x_value in sorted_data:
            f.write(f'{x_value},{",".join([data[x_value].get(y_label, "") for y_label in y_labels])}\n')
    print('Done!')


def truncate_csv_for_pgfplot(figure_name: str, csv: str, begin: int, end: int):
    '''
    Truncate a raw CSV file for plotting

    Parameters:
        figure_name (str): The output figure name
        csv (str): The required raw CSV file
        begin (int): The beginning of the range of the line number
        end (int): The end of the range of the line number
    '''

    try:
        with open(fullpath(os.path.join('results', csv)), 'rt') as f_in:
            with open(fullpath(os.path.join('results', figure_name + '.csv')), 'wt') as f_out:
                print('Reading data from', csv, 'and keep line', begin, '-', end)
                f_out.write(f_in.readline())
                for i, line in enumerate(f_in.readlines()):
                    if begin <= i <= end:
                        f_out.write(line)
        print('Done!')
    except FileNotFoundError:
        print(f'Error: {csv} not found.')
        exit(1)


if __name__ == '__main__':

    if len(argv) != 2:
        print('Usage: ./process.py [figure id]')
        exit(0)

    if argv[1] == '1':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp=['fat-tree-distributed', 'fat-tree-default'],
                                 x='cluster',
                                 y='t',
                                 legend='simulator')

    elif argv[1] == '5a':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='mpi-sync-incast',
                                 x='incast',
                                 y=['sync', 'exec', 'msg'],
                                 legend='simulator')

    elif argv[1] == '5b':
        truncate_csv_for_pgfplot(figure_name=argv[1],
                                 csv='mpi-sync.csv',
                                 begin=0,
                                 end=1000)

    elif argv[1] == '5c':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='mpi-sync-delay',
                                 x='delay',
                                 y={'ratio': lambda line, idx: str(float(line[idx['sync']]) / float(line[idx['t']]))},
                                 legend='simulator')

    elif argv[1] == '5d':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='mpi-sync-bandwidth',
                                 x=lambda line, idx: line[idx['bandwidth']].removesuffix('Gbps'),
                                 y={'ratio': lambda line, idx: str(float(line[idx['sync']]) / float(line[idx['t']]))},
                                 legend='simulator')

    elif argv[1] == '8':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='dqn',
                                 x=['cluster', 'k'],
                                 y='t',
                                 legend='simulator')

    elif argv[1] == '9':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp=['flexible', 'flexible-barrier', 'flexible-default'],
                                 x='core',
                                 y='t',
                                 y_post={'speedup-barrier': lambda data, x_value: str(float(data['']['t-default']) / float(data[x_value]['t-barrier'])) if 't-barrier' in data[x_value] else '',
                                         'speedup-unison': lambda data, x_value: str(float(data['']['t-default']) / float(data[x_value]['t-unison'])) if 't-unison' in data[x_value] else ''},
                                 legend='simulator')

    elif argv[1] == '10a':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp=['mtp-sync-incast', 'mpi-sync-incast'],
                                 x='incast',
                                 y=['sync', 'exec', 'msg'],
                                 legend='simulator')

    elif argv[1] == '10b':
        truncate_csv_for_pgfplot(figure_name=argv[1],
                                 csv='mtp-sync.csv',
                                 begin=0,
                                 end=1000)

    elif argv[1] == '11a':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp=['torus-distributed', 'torus'],
                                 x='core',
                                 y='t',
                                 legend='simulator')

    elif argv[1] == '11b':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp=['bcube', 'bcube-old', 'bcube-default'],
                                 x='cdf',
                                 y='t',
                                 y_post={'speedup-barrier': lambda data, x_value: str(float(data[x_value]['t-default-']) / float(data[x_value]['t-barrier-8'])),
                                         'speedup-nullmsg': lambda data, x_value: str(float(data[x_value]['t-default-']) / float(data[x_value]['t-nullmsg-8'])),
                                         'speedup-unison-8': lambda data, x_value: str(float(data[x_value]['t-default-']) / float(data[x_value]['t-unison-8'])),
                                         'speedup-unison-16': lambda data, x_value: str(float(data[x_value]['t-default-']) / float(data[x_value]['t-unison-16']))},
                                 legend=['simulator', 'core'])

    elif argv[1] == '11c':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='wan',
                                 x='topo',
                                 y='t',
                                 legend='simulator')

    elif argv[1] == '12':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='accuracy',
                                 x='simulator',
                                 y=['fct', 'e2ed', 'throughput'],
                                 y_post={'fct-error': lambda data, x_value: str(abs(float(data[x_value]['fct']) / float(data['default']['fct']) - 1)),
                                         'e2ed-error': lambda data, x_value: str(abs(float(data[x_value]['e2ed']) / float(data['default']['e2ed']) - 1)),
                                         'throughput-error': lambda data, x_value: str(abs(float(data[x_value]['throughput']) / float(data['default']['throughput']) - 1))})

    elif argv[1] == '13a':
        i = 2

        def counter(line, idx):
            global i
            return str((i := i + 1) // 3)

        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='deterministic',
                                 x=counter,
                                 y='ev',
                                 legend='simulator')

    elif argv[1] == '13b':
        i = 2

        def counter(line, idx):
            global i
            return str((i := i + 1) // 3)

        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='deterministic',
                                 x=counter,
                                 y='e2ed',
                                 legend='simulator')

    elif argv[1] == '14a':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='partition-cache',
                                 x='system',
                                 y=['miss', 't'])

    elif argv[1] == '14b':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='scheduling-metrics',
                                 x='core',
                                 y='slowdown',
                                 legend='sort')

    elif argv[1] == '14c':
        generate_csv_for_pgfplot(figure_name=argv[1],
                                 exp='scheduling-period',
                                 x='period',
                                 y='t')

    elif argv[1] == '15a':
        shutil.copy(fullpath('results/mpi-exec.csv'), fullpath('results/15a.csv'))

    elif argv[1] == '15b':
        shutil.copy(fullpath('results/mtp-exec.csv'), fullpath('results/15b.csv'))

    else:
        print('no such figure!')

    # compile plot.tex with pdflatex to generate plot.pdf
    subprocess.run('pdflatex -halt-on-error plot.tex', shell=True, cwd=fullpath('results'), capture_output=True)
