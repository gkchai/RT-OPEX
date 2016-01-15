#! /usr/bin/python

# statistical analysis of gd logs
# Deadline miss rate, execution time variations
# CDF of transport, processing
# TODO: statistics with priorities, scheduling, multiple cores

import numpy as np
import gd_utils as utils
import os.path

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


class gstat:
    def __init__(self, config, trans_arr, proc_arr):
        self.config = config
        self.trans = trans_arr
        self.proc = proc_arr


def read_log_timing(filenames):

    arrs = []
    for fname in filenames:
        print 'reading file %s'%fname
        arrs.append(np.loadtxt(fname, dtype={'names':('id', 'abs_period', 'abs_deadline',
            'abs_start', 'abs_end', 'rel_period', 'rel_start', 'rel_end', 'duration', 'slack'
            ), 'formats':(np.float,np.float,np.float,np.float,np.float,np.float,np.float,np.float,np.float,np.float)},
            delimiter='\t\t', skiprows=300))
        # arrs.append(np.loadtxt(fname))

    return np.concatenate(arrs)

def deadline_miss(obj):
    return (len(obj.proc[:, 'slack'] > 0)/len(obj.proc_time[:, 'slack']))


def get_cdf():
    pass


if __name__ == '__main__':

    prior_range = [1, 99]
    sched_range = ['SCHED_FIFO', 'SCHED_RR', 'SCHED_OTHER']
    nant_range = [4]
    nprocs = 3

    duration = 100 #secs

    for nants in nant_range:
        for prior in prior_range:
            for sched in sched_range:
                config  = {'prior': prior,
                            'sched' : sched,
                            'duration': duration,
                            'nant': nants,
                            'nproc':nprocs,
                            }

                filenames_t, filenames_p = [],[]
                for idx, nant in enumerate(range(nants)):
                    fname = 'log/trans%d_prior%d_sched%s_nant%d_nproc%d.log'%(idx, prior,sched, nants, nprocs)
                    if not os.path.isfile(fname):
                        print 'File %s does not exist'%fname
                        raise ValueError
                    else:
                        filenames_t.append(fname)
                for idx, nproc in enumerate(range(nprocs)):
                    fname = 'log/proc%d_prior%d_sched%s_nant%d_nproc%d.log'%(idx, prior,sched, nants, nprocs)
                    if not os.path.isfile(fname):
                        print 'File %s does not exist'%fname
                        raise ValueError
                    else:
                        filenames_p.append(fname)

                trans =  read_log_timing(filenames_t)
                proc =  read_log_timing(filenames_p)

                gs = gstat(config, trans, proc)
                utils.write_pickle(gs, 'dump/gstat_prior%d_sched%s_nant%d_nproc%d'%(prior,sched,nants, nprocs))

