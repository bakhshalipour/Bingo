from __future__ import print_function

import os
import re
import glob

# ========== SETTINGS ============

# output file name
output_file = 'figure11'

# the prefetcher you want to calculate "delta ipci" for.
# "delta ipci" is ipc improvement over best performing prior prefetcher
my_prefetcher = 'bingo'

# NOTE: leave lists empty to select all
# NOTE: strings are case sensitive
# select the workloads that you wish to evaluate (e.g. ['mix1', 'apache'])
output_workloads = [
    'cassandra',
    'cloud9',
    'em3d',
    'streaming',
    'zeus_40cl',
    'mix2',
    'mix5',
    'mix8',
    'mix13',
    'mix14'
]

# select your metrics of interest from the list of available metrics (e.g. ['IPCI', 'Coverage'])
output_metrics = [
    'IPCI',
    'Coverage',
    'Overprediction',
]

# select the prefetchers that you want to evaluate (e.g. ['ampm', 'sms'])
output_prefetchers = [
    'bop',
    'bop_deg32',
    'spp',
    'spp_alpha1',
    'vldp',
    'vldp_deg32',
    'ampm',
    'sms',
    'bingo'
]

# ================================

stats = {}
metrics = [
    'IPC',
    'IPCI',
    'Delta IPCI',
    'Instructions',
    'MPKI',
    'Accesses',
    'Misses',
    'Prefetches',
    'Prefetch Hits',
    'Non-useful Prefetches',
    'Undecided Prefetches',
    'Accuracy',
    'Coverage',
    'Uncovered',
    'Overprediction',
    'PHT Match Probability',
    'Redundancy',
    'PC+Address Prefetches',
    'PC+Offset Prefetches',
    'PC+Address Covered Misses',
    'PC+Offset Covered Misses',
    'PC+Address Overpredictions',
    'PC+Offset Overpredictions',
    'PC+Address Prefetches (%)',
    'PC+Offset Prefetches (%)',
    'PC+Address Covered Misses (%)',
    'PC+Offset Covered Misses (%)',
    'PC+Address Overpredictions (%)',
    'PC+Offset Overpredictions (%)'
]

if not output_metrics:
    output_metrics = metrics


def print_dict(d):
    import json
    print(json.dumps(d, sort_keys=True, indent=4))


def arith_mean(x):
    x = [v for v in x if v != '-']
    if not x:
        return '-'
    return 1.0 * sum(x) / len(x)


def geo_mean(x):
    x = [v for v in x if v != '-']
    if not x:
        return '-'
    prod = 1.0
    for val in x:
        prod *= val
    return prod ** (1.0 / len(x))


def add_entry(trace, prefetcher):
    global stats
    global metrics
    if trace not in stats:
        stats[trace] = {}
    if prefetcher not in stats[trace]:
        stats[trace][prefetcher] = {}
    entry = stats[trace][prefetcher]
    for field in metrics:
        if field not in entry:
            if trace == 'Average':
                entry[field] = []
            else:
                entry[field] = ['-'] * 4
    return entry


# create output folder
if not os.path.exists('out'):
    os.makedirs('out')

# extract data from files
files_4core = glob.glob('results_4core/*.txt')
files_cloud = glob.glob('results_4core_cloud/*.txt')
all_files = files_4core + files_cloud

for file in all_files:
    regex = re.search(r'[\\/](.*)-perceptron-large-no-(.*)-lru-4core.txt', file)
    if not regex:
        continue
    trace = regex.group(1)
    if output_workloads and trace not in output_workloads:
        continue
    prefetcher = regex.group(2)
    if prefetcher != 'no' and output_prefetchers and prefetcher not in output_prefetchers:
        continue
    entry = add_entry(trace, prefetcher)

    with open(file, mode='r') as f:
        for line in f.readlines():
            line = line.strip()

            regex = re.search(r'CPU ([0-3]) cumulative IPC: (.*) instructions: (.*) c', line)
            if regex:
                cpu = int(regex.group(1))
                val = float(regex.group(2))
                entry['IPC'][cpu] = val
                val = float(regex.group(3))
                entry['Instructions'][cpu] = val

            for metric in metrics:
                regex = re.search(r'\* CPU ([0-3]) ROI ' + metric + r': (.*)', line)
                if regex:
                    cpu = int(regex.group(1))
                    val = float(regex.group(2))
                    entry[metric][cpu] = val

    for cpu in range(4):
        # accuracy (= prefetch hits / (non-useful prefetches + prefetch hits))
        if entry['Prefetch Hits'][cpu] + entry['Non-useful Prefetches'][cpu] == 0:
            entry['Accuracy'][cpu] = '-'
        else:
            entry['Accuracy'][cpu] = 1.0 * entry['Prefetch Hits'][cpu] / (entry['Prefetch Hits'][cpu] + entry['Non-useful Prefetches'][cpu])
        entry['MPKI'][cpu] = 1000.0 * entry['Misses'][cpu] / entry['Instructions'][cpu]
        if entry['PC+Address Prefetches'][cpu] != '-':
            assert(entry['PC+Address Prefetches'][cpu] + entry['PC+Offset Prefetches'][cpu] == entry['Prefetches'][cpu])
            assert(entry['PC+Address Covered Misses'][cpu] + entry['PC+Offset Covered Misses'][cpu] == entry['Prefetch Hits'][cpu])
            assert(entry['PC+Address Overpredictions'][cpu] + entry['PC+Offset Overpredictions'][cpu] == entry['Non-useful Prefetches'][cpu])
            entry['PC+Address Prefetches (%)'][cpu] = entry['PC+Address Prefetches'][cpu] / entry['Prefetches'][cpu]
            entry['PC+Offset Prefetches (%)'][cpu]  = entry['PC+Offset Prefetches'][cpu]  / entry['Prefetches'][cpu]
            entry['PC+Address Covered Misses (%)'][cpu] = entry['PC+Address Prefetch Hits'][cpu] / entry['Prefetch Hits'][cpu]
            entry['PC+Offset Covered Misses (%)'][cpu]  = entry['PC+Offset Prefetch Hits'][cpu]  / entry['Prefetch Hits'][cpu]
            entry['PC+Address Overpredictions (%)'][cpu] = entry['PC+Address Non-useful Prefetches'][cpu] / entry['Non-useful Prefetches'][cpu]
            entry['PC+Offset Overpredictions (%)'][cpu]  = entry['PC+Offset Non-useful Prefetches'][cpu]  / entry['Non-useful Prefetches'][cpu]

# calculate stats relative to no prefetching baseline
for trace in stats:
    baseline = stats[trace]['no']
    for prefetcher in stats[trace]:
        entry = stats[trace][prefetcher]
        for cpu in range(4):
            scale_coef = 1.0 * baseline['Accesses'][cpu] / entry['Accesses'][cpu]
            entry['IPCI'][cpu] = 1.0 * entry['IPC'][cpu] / baseline['IPC'][cpu]
            entry['Coverage'][cpu] = 1.0 - 1.0 * entry['Misses'][cpu] / baseline['Misses'][cpu] * scale_coef
            entry['Uncovered'][cpu] = 1.0 - entry['Coverage'][cpu]
            entry['Overprediction'][cpu] = 1.0 * entry['Non-useful Prefetches'][cpu] / baseline['Misses'][cpu] * scale_coef

# take average stats of all cores
for trace in stats:
    for prefetcher in stats[trace]:
        entry = stats[trace][prefetcher]
        for field in metrics:
            if field == 'IPC':
                entry[field] = geo_mean(entry[field])
            else:
                entry[field] = arith_mean(entry[field])

# calculate average statistics
stats['Average'] = {}
for trace in stats:
    if trace == 'Average':
        continue
    for prefetcher in stats[trace]:
        entry = stats[trace][prefetcher]
        avg_entry = add_entry('Average', prefetcher)
        for field in metrics:
            avg_entry[field].append(entry[field])

for prefetcher in stats['Average']:
    entry = stats['Average'][prefetcher]
    for field in metrics:
        if field == 'IPCI':
            entry[field] = geo_mean(entry[field])
        else:
            entry[field] = arith_mean(entry[field])

# calculate delta ipci for all workloads and average
for trace in stats:
    max_ipci = 0
    if my_prefetcher not in stats[trace]:
        continue
    for prefetcher in stats[trace]:
        entry = stats[trace][prefetcher]
        if prefetcher == my_prefetcher:
            continue
        max_ipci = max(max_ipci, entry['IPCI'])
    entry = stats[trace][my_prefetcher]
    entry['Delta IPCI'] = entry['IPCI'] - max_ipci

# output stats as csv file
with open('out/' + output_file + '.csv', 'w') as f:
    print(','.join(['Trace', 'Prefetcher'] + output_metrics), file=f)
    row = []
    for trace in ['Average'] + output_workloads:
        row.append(trace)
        for prefetcher in output_prefetchers:
            row.append(prefetcher)
            entry = stats[trace][prefetcher]
            for field in output_metrics:
                row.append(entry[field])
            print(','.join([str(x) for x in row]), file=f)
            for field in output_metrics:
                row.pop()
            row.pop()
        row.pop()
