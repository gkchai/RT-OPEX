#! /usr/bin/python

# statistical analysis of gd logs
# Deadline miss rate, execution time variations
# CDF of transport, processing
# TODO: statistics with priorities, scheduling, multiple cores

import numpy as np
import gd_utils as utils
import os.path
from pprint import pprint
import pdb
import gd_lte_utils as lte_utils

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



def read_proc_log(filenames):
    arrs = []
    for fname in filenames:
        # print 'reading file %s'%fname
        arr = np.loadtxt(fname, dtype={'names':('mcs', 'abs_period', 'abs_deadline',
            'abs_start', 'abs_end', 'rel_period', 'rel_start', 'rel_end', 'duration',
            'miss', 'iter', 'kill', 'migrated'
            ), 'formats':(np.int,np.int64,np.int64,np.int64,np.int64,np.int64,np.int64,np.int64,np.int,
            np.int,np.int,np.int,np.int)},
            delimiter='\t', skiprows=500)
        arrs.append(arr[:-500])
    return np.concatenate(arrs)




# get the proc stats for given array
def time_analysis(arrp, rtt):

    # deadline-miss rate
    miss_rate = sum(arrp['miss'])*1.0/len(arrp['miss'])
    print len(arrp)

    miss_rate = 0

    # throughput
    thru = 0
    nitem = 0
    thru_all = []
    for item in arrp:

        if item['mcs'] >= 0:

            curr_thru =  lte_utils.mcs_to_throughput(item['mcs'])*(1 if ((item['iter'] < 5) and (item['iter'] > -1) and (item['duration'] < (2000-rtt))) else 0)
            thru += curr_thru
            # miss_rate += (1 if ((item['duration'] > (2000-rtt) and item['iter'] < 5) or (item['iter'] == -1)) else 0)
            miss_rate += (1 if ((item['duration'] > (2000-rtt) ) or (item['iter'] == -1)) else 0)
            nitem += 1
        # if miss_rate > 0:
        #     pdb.set_trace()
            thru_all.append(curr_thru)

    # thru = thru*1.0/len(arrp)
    # miss_rate = miss_rate*1.0/len(arrp)

    print nitem
    thru = thru*1.0/nitem
    miss_rate = miss_rate*1.0/nitem


    # get cdf of throughput
    obj = astat(thru_all)
    thru_cdf = []
    for x in np.arange(0, 35, 0.2):
        thru_cdf.append(obj.get_cdf(x))

    # pprint(result)
    return [miss_rate, thru, thru_cdf]

# requires all logs to calculate cpu util
def util_analysis(arrps):

    cpu_u, cpu_u_orig, dead_miss_orig, dead_miss = [], [], [], []
    for ix, arrp in enumerate(arrps):

        original_deadline_misses = len(arrp[(arrp['abs_deadline'] - arrp['abs_start'] < arrp['original_duration'])])*100.0/len(arrp)
        deadline_misses = len(arrp[arrp['miss']==1])*100.0/len(arrp)

        sum_my_offload_task_dur = sum(arrps[(ix+1)%3]['dur_offload'])

        exp_dur = arrp['abs_end'][-1] - arrp['abs_start'][0]
        original_cpu_util = sum(np.minimum(arrp['original_duration'], arrp['abs_deadline'] - arrp['abs_start']))*100.0/exp_dur
        cpu_util = (sum_my_offload_task_dur + sum(np.minimum(arrp['duration'], arrp['abs_deadline'] - arrp['abs_start'])))*100.0/exp_dur

        print 'CPU %d, Util %2.2f --> %2.2f, Deadline miss %2.5f --> %2.5f'%(ix, original_cpu_util, cpu_util, original_deadline_misses, deadline_misses)

        cpu_u_orig.append(original_cpu_util)
        cpu_u.append(cpu_util)
        dead_miss_orig.append(original_deadline_misses)
        dead_miss.append(deadline_misses)


    result = [cpu_u, cpu_u_orig, dead_miss_orig, dead_miss]

    return result


def main(exp, samples, nants, nprocs, prior, sched, T_T, T_P, N_P):


    arrts = []
    for idx, nant in enumerate(range(nants)):
        fname = '../log/exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior,sched, nants, nprocs)
        if not os.path.isfile(fname):
            print 'File %s does not exist'%fname
            raise ValueError
        else:
            arr = read_log_timing([fname])
            utils.write_pickle(arr, '../dump/gstat_exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d_T_T%d_T_P%d_N_P%d'%(exp, samples, idx, prior, sched,nants, nprocs, T_T, T_P, N_P))

        arrts.append(arr)
        print 'avg transport duration radio %d is %f us %f'%(idx, np.mean(arr['duration']), np.std(arr['duration']))


    arrps, arros = [],[]
    avg_procs, avg_offloads = [], []
    for idx, nproc in enumerate(range(nprocs)):
        fname = '../log/exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior, sched, nants, nprocs)

        fname_o = '../log/exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d.log'%(exp, samples, idx, prior, sched, nants, nprocs)


        if (not os.path.isfile(fname)) or (not os.path.isfile(fname_o)):
            print 'File %s does not exist'%fname
            raise ValueError
        else:
            arrp = read_proc_log_timing([fname])
            arro = read_off_log_timing([fname_o])
            utils.write_pickle(arrp, '../dump/gstat_exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d_T_T%d_T_P%d_N_P%d'%(exp, samples, idx, prior, sched,nants, nprocs, T_T, T_P, N_P))
            utils.write_pickle(arro, '../dump/gstat_exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d_T_T%d_T_P%d_N_P%d'%(exp, samples, idx, prior, sched,nants, nprocs, T_T, T_P, N_P))


        # analyse the logs
        res, arrp, arro, avg_proc, avg_offload = time_analysis(arrp, arro)

        arrps.append(arrp)
        arros.append(arro)
        avg_procs.append(avg_proc)
        avg_offloads.append(avg_offload)

    res_u = util_analysis(arrps)
    return res_u, np.mean(avg_procs), np.mean(avg_offloads)



if __name__ == '__main__':


    exp_range = ['offload']
    samples_range = [15000]
    prior_range = [10]
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

                        main(exp, samples, nants, prior, nprocs, sched, T_T, T_P, N_P)

