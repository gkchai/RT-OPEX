import os
import subprocess
from gd_stats import main, util_analysis, time_analysis
from gd_plot_config import plt, PdfPages, nospines
import gd_utils as utils
import numpy as np

bitspsym = [ 0.1647619 ,  0.21428571,  0.26380952,  0.34,  0.43142857,
        0.52285714,  0.61428571,  0.73809524,  0.82952381,  0.95142857,
        0.95142857,  1.04285714,  1.18      ,  1.36285714,  1.54285714,
        1.68      ,  1.81714286,  1.81714286,  1.95428571,  2.18285714,
        2.31952381,  2.54571429,  2.72857143,  3.03047619,  3.25904762,
        3.37333333,  3.64      ,  3.77428571]

qfactor = [2,2,2,2,2,
          2,2,2,2,2,
          4,4,4,4,4,
          4,4,6,6,6,
          6,6,6,6,6,
          6,6,6]

dur = 100
T_P = 93
n_ants = 2
N_P = n_ants
turbo_iter = 2


# no of BSs that are processed together
pooling = 1

# mcs_range = [5, 10, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]
mcs_range = [16]

def mcs_to_turbo(mcs):
    return int(bitspsym[mcs]*96.8)

def mcs_to_time(mcs):
    dl_time = 0
    return int(bitspsym[mcs]*96.8*turbo_iter + 43.9*qfactor[mcs] + 93.0*n_ants + 28.4 + dl_time)

def mcs_to_throughput(mcs):
    return int(bitspsym[mcs]*(14*12*50)*1.0/10**3)


exp_range = ['offload']
samples_range = [15000]
prior_range = [10]
# sched_range = ['SCHED_FIFO', 'SCHED_RR', 'SCHED_OTHER']
sched_range = ['SCHED_FIFO']
nant_range = [n_ants]
nprocs = 3

# proc time for mcs
t_t_range = []
for mcs in mcs_range:
    t_t_range.append(mcs_to_time(mcs))

# throughput for mcs
th_range = []
for mcs in mcs_range:
    th_range.append(mcs_to_throughput(mcs))


cpu_u_orig, dead_miss_orig, res_us = [], [], []
avg_procs, avg_offloads = [], []

arrts = []
for mcs in mcs_range:
    T_T = mcs_to_time(mcs)

    print "MCS =", mcs

    for exp in exp_range:
        for samples in samples_range:
            for nants in nant_range:
                for prior in prior_range:
                    for sched in sched_range:

                        # run the C code
                        args = ' -n %d -s 15000 -d %d -p 10 -S F -e O -D 0 -X %d -Y %d -Z %d'%(nants, dur, T_T, T_P, N_P)
                        print args
                        subprocess.check_call('../src/gd.o'+args, shell=True)
                        _,avg_proc,avg_offload =  main(exp, samples, nants, nprocs, prior, sched, T_T, T_P, N_P)

                        for idx, ntrans in enumerate(range(nants)):

                            arrt = utils.read_pickle('../dump/gstat_exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d_T_T%d_T_P%d_N_P%d'%(exp, samples, idx, prior, sched,nants, nprocs, T_T, T_P, N_P))


                        arrps, arros = [], []
                        for idx, nproc in enumerate(range(nprocs)):


                            arrp = utils.read_pickle('../dump/gstat_exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d_T_T%d_T_P%d_N_P%d'%(exp, samples, idx, prior, sched,nants, nprocs, T_T, T_P, N_P))

                            arro = utils.read_pickle('../dump/gstat_exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d_T_T%d_T_P%d_N_P%d'%(exp, samples, idx, prior, sched,nants, nprocs, T_T, T_P, N_P))

                            # analyse the logs
                            res, arrp, arro, _, _ = time_analysis(arrp, arro)

                            arrts.append([arrt['duration']])
                            arrps.append(arrp)
                            arros.append(arro)


                        res_u = util_analysis(arrps)
                        res_us.append(res_u)

                        cpu_u_orig.append(sum(res_u[1]))
                        dead_miss_orig.append(np.mean(res_u[2]))
                        avg_procs.append(avg_proc)
                        avg_offloads.append(avg_offload)


# utils.write_pickle(res_us, '../dump/res_us_%d.pkl'%n_ants)
# utils.write_pickle(arrts, '../dump/trans_%d.pkl'%n_ants)
utils.write_pickle([avg_procs, avg_offloads], '../dump/proc_offload_%d.pkl'%n_ants)
