#! /usr/bin/python

# statistical analysis of gd logs
# Deadline miss rate, execution time variations
# CDF of transport, processing
# TODO: statistics with priorities, scheduling, multiple cores

import numpy as np
import gd_utils as utils
import os.path

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

# gd stats
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
        arrs.append(np.loadtxt(fname, dtype={'names':('id', 'abs_period', 'abs_deadline',
            'abs_start', 'abs_end', 'rel_period', 'rel_start', 'rel_end', 'duration', 'slack'
            ), 'formats':(np.int32,np.int32,np.int32,np.int32,np.int32,np.int32,np.int32,np.int32,np.int32,np.int32)},
            delimiter='\t\t', skiprows=300))
    return np.concatenate(arrs)

def read_off_log_timing(filenames):

    arrs = []
    for fname in filenames:
        print 'reading file %s'%fname
        arrs.append(np.loadtxt(fname, dtype={'names':('id', 'rel_period', 'rel_start',
            'rel_end', 'rel_task_start', 'rel_task_end', 'total_duration', 'task_duration'
            ), 'formats':(np.int32,np.int32,np.int32,np.int32,np.int32,np.int32,np.int32,np.int32)},
            delimiter='\t\t', skiprows=300))
    return np.concatenate(arrs)




def deadline_miss(obj):
    return (len(obj.proc[:, 'slack'] > 0)/len(obj.proc_time[:, 'slack']))


def get_cdf():
    pass


if __name__ == '__main__':

    exp_range = ['plain', 'offload']
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

                        filenames_t, filenames_p = [],[]
                        for idx, nant in enumerate(range(nants)):
                            fname = '../log/exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior,sched, nants, nprocs)
                            if not os.path.isfile(fname):
                                print 'File %s does not exist'%fname
                                raise ValueError
                            else:
                                filenames_t.append(fname)
                        for idx, nproc in enumerate(range(nprocs)):
                            fname = '../log/exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx prior, sched, nants, nprocs)

                            fname_o = '../log/exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx prior, sched, nants, nprocs)


                            if (not os.path.isfile(fname)) or (not os.path.isfile(fname_o)):
                                print 'File %s does not exist'%fname
                                raise ValueError
                            else:
                                filenames_p.append(fname)
                                filenames_o.append(fname_o)


                        trans =  read_log_timing(filenames_t)
                        proc =  read_log_timing(filenames_p)
                        offload = read_off_log_timing(filenames_o)

                        gs = gstat(config, trans, proc)
                        utils.write_pickle(gs, '../dump/gstat_exp%s_samp%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, prior,sched,nants, nprocs))