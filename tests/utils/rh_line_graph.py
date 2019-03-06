#!/usr/bin/python3

#pip3 install matplotlib
#sudo apt-get install python3-tk

import matplotlib.pyplot as plt
import sys, getopt
import json
import requests
import time
import sys
import urllib3
from urllib.parse import urlencode
stord_ip = '127.0.0.1:9000'

json_vmdk_list = []

def print_progress_bar(iteration, total, prefix = '', suffix = '', decimals = 1, length = 100, fill = '█'):
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print('\r%s |%s| %s%% %s' % (prefix, bar, percent, suffix), end = '\r')
    # Print New Line on Complete
    if iteration == total:
        print()

def run_stord_stats(vmdk_id, num_samples, time_interval):
    h = "http"
    cert = None
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    headers = {'Content-type': 'application/json'}
    data = { "service_type": "test_server", "service_instance" : 0, "etcd_ips" : ["3213213", "213213"]}
    # Get vmdk stats
    print("Getting vmdk stats from StorD for %s seconds" % (num_samples * time_interval))
    print_progress_bar(0, num_samples, prefix = 'Progress:', suffix = 'Complete', length = 50)
    for x in range(0, num_samples) :
        r = requests.get("%s://%s/stord_svc/v1.0/vmdk_stats/?vmdk-id=%s" % (h, stord_ip, vmdk_id), data=json.dumps(data), headers=headers, cert=cert, verify=False)
        assert (r.status_code == 200)
        json_vmdk_list.append(r.json())
        progress = (100 / num_samples)*(x+1)
        print_progress_bar(x+1, num_samples, prefix = 'Progress:', suffix = 'Complete', length = 50)
        time.sleep(time_interval)

def plot_line_graph(time, read_misses, read_hits, rh_blocks):
    plt.plot(time, read_misses, color='red')
    plt.plot(time, read_hits, color='green')
    plt.plot(time, rh_blocks, color='blue')
    plt.xlabel('Time(Seconds)')
    plt.ylabel('Number Of Blocks')
    plt.title('ReadAhead Impact Analysis')
    plt.legend(('Read Misses', 'Read Hits', 'Read Ahead'), loc='upper right')
    plt.show()

def get_samples(opt, json_list):
    key = ''
    samples = []
    if opt == 'r':
        key = 'read_ahead_blks'
    elif opt == 'm':
        key = 'read_miss'
    elif opt == 'h':
        key = 'read_hits'
    elif opt == 't':
        key = 'total_reads'
    for an_item in json_list:
        samples.append(an_item[key])

    return samples

def get_read_misses(json_list):
    return get_samples('m', json_list)

def get_read_hits(json_list):
    return get_samples('h', json_list)

def get_rh_blocks(json_list):
    return get_samples('r', json_list)

def get_total_reads(json_list):
    return get_samples('t', json_list)

def get_time_samples(num_samples, time_interval):
    time_samples = []
    time_slice = 0 - time_interval
    for x in range(0, num_samples):
        time_slice += time_interval
        time_samples.append(time_slice)
    return time_samples

def show_usage():
    print('rh_line_graph <vmdk_id_int> <num_samples_int> <sampling_interval_sec>')
    sys.exit(2)

def main(argv):
    if len(argv) != 3:
        show_usage()
    vmdk_id = int(argv[0])
    num_samples = int(argv[1])
    time_interval = int(argv[2])

    run_stord_stats(vmdk_id, num_samples, time_interval)
    time = get_time_samples(num_samples, time_interval)
    read_hits = get_read_hits(json_vmdk_list)
    rh_blocks = get_rh_blocks(json_vmdk_list)
    read_misses = get_read_misses(json_vmdk_list)
    total_reads = get_total_reads(json_vmdk_list)
    print("Data Set for plotting")
    print("time = %s" % time)
    print("Read Misses = %s" % read_misses)
    print("Read Hits = %s" % read_hits)
    print("Read Ahead = %s" % rh_blocks)
    print("Total Reads = %s" % total_reads)
    # Plot the graph
    print('Plotting graph...')
    plot_line_graph(time, read_misses, read_hits, rh_blocks)

if __name__ == "__main__":
    main(sys.argv[1:])
