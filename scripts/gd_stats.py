#! /usr/bin/python

# statistical analysis of gd logs
# Deadline miss rate, execution time variations
# CDF of transport, processing
# TODO: statistics with priorities, scheduling, multiple cores

import numpy as np
import gd_utils as utils
import os.path
from pprint import pprint

# array stats
class astat:
    def __init__(self, arr):
        self.arr = arr
        self.bins = np.linspace(np.min(self.arr), np.max(self.arr), 1000)
        self.pdf = None
        self.cdf = None

    def get_pdf(self):
        self.pdf = utils.pdf(self.arr, self.bins)
        return self.pdf

    def get_cdf(self, x):
        self.cdf = utils.pdftocdf(self.get_pdf(), self.bins)
        return self.cdf(x)

# exp stats
class gstat:
    def __init__(self, config, trans_arr, proc_arr, offload_arr):
        self.config = config
        self.trans = trans_arr
        self.proc = proc_arr
        self.offload = offload_arr


def read_log_timing(filenames):
    arrs = []
    for fname in filenames:
        print 'reading file %s'%fname
        arr = np.loadtxt(fname, dtype={'names':('id', 'abs_period', 'abs_deadline',
            'abs_start', 'abs_end', 'rel_period', 'rel_start', 'rel_end', 'duration', 'slack'
            ), 'formats':(np.int64,np.int64,np.int64,np.int64,np.int64,np.int64,np.int64,np.int64,np.int64,np.int64)},
            delimiter='\t\t', skiprows=300)
        arrs.append(arr[:-100])
    return np.concatenate(arrs)

def read_off_log_timing(filenames):
    arrs = []
    for fname in filenames:
        print 'reading file %s'%fname
        arr = np.loadtxt(fname, dtype={'names':('id', 'abs_period', 'abs_start', 'abs_end',
         'abs_task_start', 'abs_task_end','rel_period', 'rel_start',
            'rel_end', 'rel_task_start', 'rel_task_end', 'total_duration', 'task_duration', 'type'
            ), 'formats':(np.int64, np.int64, np.int64,np.int64,np.int64,np.int64,np.int64, np.int64,np.int64,np.int64,np.int64,np.int64,np.int64, 'S20')},
            delimiter='\t\t', skiprows=300)
        arrs.append(arr[:-100])
    return np.concatenate(arrs)


def correctness(arrp, arro):

    nitems = min(len(arrp), len(arro))
    correct = True
    for i in range(nitems):
        ps = arrp['abs_start'][i]
        pe = arrp['abs_end'][i]
        os = arro['abs_start'][i]
        oe = arro['abs_end'][i]
        if  (ps > os and ps < oe) or (os > ps and os < pe):
            correct = False
            print "proc=[",ps, pe,"] off=[", os, oe,"]"


            break
    return correct


# get the offload + proc stats
def time_analysis(arrp, arro):

    start_time = max(arrp['abs_start'][0], arro['abs_start'][0])
    end_time = min(arrp['abs_start'][-1], arro['abs_start'][-1])

    arrp = arrp[(arrp['abs_start'] >= start_time) & (arrp['abs_start'] <= end_time)]
    arro = arro[(arro['abs_start'] >= start_time) & (arro['abs_start'] <= end_time)]

    print "items: proc = %d, offload = %d"%(len(arrp), len(arro))

    assert(correctness(arrp, arro))

    # Proc then Off
    sum_proc_dur = sum(arrp['duration'])
    sum_offload_dur = sum(arro['total_duration'])

    # consider only cases that have task complete
    arro_t = arro[arro['type'] == 'task_complete']

    sum_offload_task_dur = sum(arro_t['task_duration'])
    overall_dur = arrp['abs_end'][-1] - arrp['abs_start'][0]

    result = {  'overall_dur_ms': overall_dur*1.0/1000,
                'time_proc_ms': sum_proc_dur*1.0/1000,
                'time_offload_ms': sum_offload_dur*1.0/1000,
                'time_offload_task_ms': sum_offload_task_dur*1.0/1000,
                }

    pprint(result)
    return result


if __name__ == '__main__':

    exp_range = ['offload']
    samples_range = [1000]
    prior_range = [99]
    # sched_range = ['SCHED_FIFO', 'SCHED_RR', 'SCHED_OTHER']
    sched_range = ['SCHED_FIFO']
    nant_range = [4]
    nprocs = 3

    duration = 100 #secs

    for exp in exp_range:
        for samples in samples_range:
            for nants in nant_range:
                for prior in prior_range:
                    for sched in sched_range:
                        config  = {
                                    'exp': exp,
                                    'samples': samples,
                                    'prior': prior,
                                    'sched' : sched,
                                    'duration': duration,
                                    'nant': nants,
                                    'nproc':nprocs,
                                    }

                        for idx, nant in enumerate(range(nants)):
                            fname = '../log/exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior,sched, nants, nprocs)
                            if not os.path.isfile(fname):
                                print 'File %s does not exist'%fname
                                raise ValueError
                            else:
                                arr = read_log_timing([fname])
                                utils.write_pickle(arr, '../dump/gstat_exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, idx, prior,sched,nants, nprocs))


                        for idx, nproc in enumerate(range(nprocs)):
                            fname = '../log/exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior, sched, nants, nprocs)

                            fname_o = '../log/exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior, sched, nants, nprocs)


                            if (not os.path.isfile(fname)) or (not os.path.isfile(fname_o)):
                                print 'File %s does not exist'%fname
                                raise ValueError
                            else:
                                arr_p = read_log_timing([fname])
                                arr_o = read_off_log_timing([fname_o])
                                utils.write_pickle(arr_p, '../dump/gstat_exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, idx, prior,sched,nants, nprocs))
                                utils.write_pickle(arr_o, '../dump/gstat_exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, idx, prior,sched,nants, nprocs))


                            # analyse the logs
                            res = time_analysis(arr_p, arr_o)






