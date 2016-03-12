import os
import subprocess
from gd_stats_new import main, util_analysis, time_analysis, read_proc_log
from gd_plot_config import plt, PdfPages, nospines
import gd_utils as utils
import gd_lte_utils as lte_utils
import numpy as np
import math
from datetime import datetime


# mcs_range = [5, 10, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]
mcs_range = [20]
# exp_range = ['static', 'static2', 'migrate', 'global']
exp_range = ['static', 'static2','migrate']
# exp_range = ['global']
samples = 15000
prior = 10
sched = 'SCHED_FIFO'
# num_bss_range = [1,2,3,4,5,6,7,8]
num_bss_range = [4]
num_ants_range = [2]
lmax_range = [1500]
rtt_range = range(300, 620, 20)
# rtt_range = range(400, 550, 50)
# rtt_range = [700]
var_range = [1]
snr_range = [30]
# snr_range = [30]

run = 0

# proc time for mcs
t_t_range = []
for mcs in mcs_range:
    t_t_range.append(lte_utils.mcs_to_time(mcs,1))

# throughput for mcs
th_range = []
for mcs in mcs_range:
    th_range.append(lte_utils.mcs_to_throughput(mcs))



for mcs in mcs_range:
    for exp in exp_range:
        for num_bss in num_bss_range:
            for num_ants in num_ants_range:
                for lmax in lmax_range:
                    for rtt in rtt_range:
                        for var in var_range:
                            for snr in snr_range:

                                nprocs = num_bss*int(math.ceil(1.0*lmax/1000))
                                num_cores_bs = int(math.ceil(1.0*lmax/1000))
                                # global is not dependent on input parameters
                                max_cores = int(math.ceil(1.0*lmax/1000))*num_bss
                                # print str(datetime.now())

                                if run == 1:

                                    if exp == 'global':
                                        # for num_cores in range(1, 2*max_cores+1, 1):
                                        for num_cores in [max_core, 2*max_cores]:
                                            args = ' -M %d -A %d -L %d -s 1000 -d 30 -p 10 -S F -e %d -D 1 -N %d -m %d -R %d -C %d'%(num_bss, num_ants, lmax, var, snr, mcs, rtt, num_cores)
                                            print exp+args
                                            subprocess.check_call('sudo ../src/gd_lte_%s.o'%exp+args, shell=True)
                                    else:
                                        # run the C code
                                        args = ' -M %d -A %d -L %d -s 1000 -d 30 -p 10 -S F -e %d -D 1 -N %d -m %d -R %d'%(num_bss, num_ants, lmax, var, snr, mcs, rtt)
                                        print  exp+args
                                        subprocess.check_call('sudo ../src/gd_lte_%s.o'%exp+args, shell=True)

                                else:
                                    if exp == 'global':
                                        # core_range = range(1, (2*max_cores+1),1)
                                        core_range = [max_cores, 2*max_cores]
                                    else:
                                        core_range = [max_cores]

                                    for cores in core_range:

                                        fnames = []
                                        for idx, nproc in enumerate(range(cores)):

                                            if exp == 'global':
                                                fname = '../log_%s/exp%d_samp1000_proc%d_prior10_sched%s_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d.log'%(exp, var, idx, sched, num_bss, num_ants, cores, lmax, snr, mcs, rtt)

                                            else:
                                                fname = '../log_%s/exp%d_samp1000_proc%d_prior10_sched%s_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d.log'%(exp, var, idx, sched, num_bss, num_ants, num_cores_bs, lmax, snr, mcs, rtt)

                                            if not os.path.isfile(fname):
                                                print 'File %s does not exist'%fname
                                                raise ValueError

                                            fnames.append(fname)



                                        ress = {}
                                        # analyse the logs to get deadline misses and cpu utilization
                                        arrps = read_proc_log(fnames)
                                        exp_str = 'exp%s_var%d_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d'%(exp, var,num_bss, num_ants, cores, lmax, snr, mcs, rtt)

                                        res = time_analysis(arrps, rtt)
                                        ress[exp_str] = res
                                        print exp_str
                                        print 'Miss rate = %.3e, thru = %2.2f mbps'%(res[0], res[1])
                                        print '----------------------------------------------------'

                                        utils.write_pickle(ress, '../dump/ress11_%s.pkl'%(exp_str))

                        # ress.append(res)
                        # res_us.append(res_u)