from gd_plot_config import plt, PdfPages, nospines, colors_e,ms,fmts,ls
import gd_utils as utils
import numpy as np
import math

if __name__ == '__main__':

    from gd_stats import gstat, astat


    exp_range = ['static', 'static2', 'migrate', 'global']
    sched_range = ['SCHED_FIFO']
    nant_range = [1]
    mcs_range = [20]
    exp_range = ['static', 'static2', 'migrate', 'global']
    # exp_range = ['global']
    samples = 15000
    prior = 10
    sched = 'SCHED_FIFO'
    # num_bss_range = [1,2,3,4,5,6,7,8]
    num_bss_range = [4]
    num_ants_range = [1]
    lmax_range = [1500]
    rtt_range = range(300, 620, 20)
    # rtt_range = [600]
    var_range = [1]
    snr_range = [20,30]
    exp_range_str = ['Partitioned (8 cores)', 'Partitioned-P(16 cores)', 'RT-OPEX(8 cores)', 'Global(8 cores)', 'Global (16 cores)']


    # miss rate vs load for different schedulers

    # 1: miss rate vs rtt for different schedulers, varying load
    snr = 30
    var = 1
    lmax = 1500
    mcs=20
    num_ants = 2
    num_bss = 4


    # for num_bss in num_bss_range:
    for num_bss in [4]:

        nprocs = int(num_bss*math.ceil(1.0*lmax/1000))
        num_cores_bs = int(math.ceil(1.0*lmax/1000))
        max_cores = int(math.ceil(1.0*lmax/1000))*num_bss


        y1, y11 = [], []
        for exp in exp_range:
            y,z = [],[]
            for rtt in rtt_range:

                exp_str = 'exp%s_var%d_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d'%(exp, var,num_bss, num_ants, max_cores, lmax, snr, mcs, rtt)
                ress = utils.read_pickle('../dump/ress11_%s.pkl'%(exp_str))
                # if ress[exp_str][0] < 0.0001:
                #     ress[exp_str][0] = 0

                y.append(ress[exp_str][0])
                z.append(ress[exp_str][1])
            y1.append(y[:])
            y11.append(z[:])

        # global 16 cores
        y,z = [],[]
        for rtt in rtt_range:
            exp_str = 'exp%s_var%d_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d'%('global', var,num_bss, num_ants, 16, lmax, snr, mcs, rtt)
            ress = utils.read_pickle('../dump/ress11_%s.pkl'%(exp_str))
            y.append(ress[exp_str][0])
            z.append(ress[exp_str][1])
        y1.append(y[:])
        y11.append(z[:])

       #############
        # plot 1
        # fig = plt.figure(figsize=(4*1.7, 3*1.7))
        fig = plt.figure(figsize=(8*1.7, 3*1.7))
        ax = plt.gca()

        # for ix, exp in enumerate(exp_range):
        for ix, exp in enumerate(exp_range_str):
            # plt.plot(rtt_range, y1[ix], ls[ix], color=colors_e[ix], mfc='none', label=exp_range_str[ix], ms=14, mec=colors_e[ix], linewidth=4, markeredgewidth=4)

            # if exp == 'static' or exp == 'static2':
            plt.plot(rtt_range, y1[ix], ls[ix], color=colors_e[ix], mfc='none', label=exp_range_str[ix], linewidth=4)

        # plt.grid()
        ax.set_yscale('log')
        plt.xlabel('RTT/2 (us)')
        # plt.xlabel('MCS')
        plt.ylabel('Deadline miss rate')
        plt.ylim(0.001,1.1)
        plt.xlim(350, 600)
        nospines(ax)

        # lg=ax.legend(loc='lower right',numpoints=1, fontsize=20)
        lg=ax.legend(loc='upper left',numpoints=1, fontsize=20)
        if lg is not None:
            lg.draw_frame(False)

        plt.grid()
        pp = PdfPages('../plot/miss_rtt_all_var_bs%d.pdf'%num_bss)
        pp.savefig(bbox_inches='tight',  dpi=300)
        pp.close()
        plt.close(fig)

   #############



    #2: miss rate vs rtt for different schedulers, fixed load
    num_bss = 4
    max_cores = int(math.ceil(1.0*lmax/1000))*num_bss
    y2 = []
    for exp in exp_range:
        y = []
        for rtt in rtt_range:
            exp_str = 'exp%s_var%d_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d'%(exp, 1,num_bss, num_ants, max_cores, lmax, snr, mcs, rtt)
            ress = utils.read_pickle('../dump/ress11_%s.pkl'%(exp_str))
            y.append(ress[exp_str][0])
        y2.append(y[:])


    #############################
    # plot 2

    fig = plt.figure(figsize=(4*1.7, 3*1.7))
    ax = plt.gca()

    for ix, exp in enumerate(exp_range):
        plt.plot(rtt_range, y2[ix], '-o', color=colors_e[ix], mfc='none', label=exp_range_str[ix], ms=14, mec=colors_e[ix], linewidth=4, markeredgewidth=4)

    # plt.grid()
    ax.set_yscale('log')
    plt.xlabel('RTT/2 (us)')
    # plt.xlabel('MCS')
    plt.ylabel('Deadline miss rate')
    plt.ylim(0.001,1.2)
    plt.xlim(400, 900)
    nospines(ax)

    # lg=ax.legend(loc='lower right',numpoints=1, fontsize=20)
    # if lg is not None:
    #     lg.draw_frame(False)

    plt.grid()
    pp = PdfPages('../plot/miss_rtt_all_fixed.pdf')
    pp.savefig(bbox_inches='tight',  dpi=300)
    pp.close()
    plt.close(fig)





    ##############################################


    rtt = 400
    snr = 30
    #3: global scheduler: miss_rate vs #cores for different BSs
    y3 = []
    for rtt in range(400, 550, 50):
        y = []
        max_cores = int(math.ceil(1.0*lmax/1000))*num_bss
        # for cores in range(num_bss, max_cores+1, 1):
        for cores in range(1, 2*max_cores+1, 1):
            exp_str = 'exp%s_var%d_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d'%('global', var,num_bss, num_ants, cores, lmax, snr, mcs, rtt)
            ress = utils.read_pickle('../dump/ress11_%s.pkl'%(exp_str))
            y.append(ress[exp_str][0])
        y3.append(y[:])




    #############################
    # plot 3

    fig = plt.figure(figsize=(4*1.7, 3*1.7))
    ax = plt.gca()
    # for ix, num_bss in enumerate(num_bss_range):
    # for ix, num_bss in enumerate([4]):
    for ix, rtt in enumerate(range(400, 550, 50)):

        max_cores = int(math.ceil(1.0*lmax/1000))*num_bss
        core_range = range(1, 2*max_cores+1, 1)
        plt.plot(core_range, y3[ix], ls[ix], color=colors_e[ix], mfc='none', label='%d us'%rtt, ms=14, mec=colors_e[ix], marker = ms[ix], linewidth=4, markeredgewidth=4)

    # plt.grid()
    ax.set_yscale('log')
    plt.xlabel('#cores')
    # plt.xlabel('MCS')
    plt.ylabel('Deadline miss rate')
    plt.ylim(0.001,1.1)
    plt.xlim(0, 16)
    nospines(ax)

    # lg=ax.legend(loc='upper left',numpoints=1)
    # if lg is not None:
    #     lg.draw_frame(False)

    plt.grid()
    pp = PdfPages('../plot/miss_cores_global_var.pdf')
    pp.savefig(bbox_inches='tight',  dpi=300)
    pp.close()
    plt.close(fig)




    ##############################
    # Plot 4
    # cdf of thrroughput






    ##############################
    # cdf duration of transport, processing, offload

    # stats = {}
    # for exp in exp_range:
    #     for samples in samples_range:
    #         for nants in nant_range:

    #             obj=utils.read_pickle('../dump/gstat_exp%s_samp%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, prior,sched,nants, nprocs))
    #             stats[exp+str(samples)+str(nants)]=[astat(obj.trans['duration']), astat(obj.proc['duration']), astat(obj.offload['duration'])]

    # # proc duration vs exp
    # fig = plt.figure(figsize=(4*1.5, 3*1.5))
    # ax = plt.gca()
    # bins = np.linspace(0, 2000, 1000)
    # samples = 1000
    # nants = 4
    # dstr = 'plain'+str(samples)+str(nants)
    # plt.plot(bins, stats[dstr][1].get_cdf(bins), linewidth=4, label='FIFO')
    # dstr = 'offload'+str(samples)+str(nants)
    # plt.plot(bins, stats[dstr][1].get_cdf(bins), linewidth=4, label='RR', color='#999fff')

    # # ax.set_yticks(np.arange(0,6,1))
    # plt.xlabel('Duration (us)')
    # plt.ylabel('CDF')
    # plt.ylim(0,1)
    # nospines(ax)
    # plt.grid()
    # pp = PdfPages('plot/cdf_proc_dur_exp.pdf')
    # pp.savefig( bbox_inches='tight', dpi=300)
    # pp.close()
    # plt.close(fig)

    # # trans duration vs samples, nants
    # fig = plt.figure(figsize=(4*1.5, 3*1.5))
    # ax = plt.gca()
    # bins = np.linspace(0, 2000, 1000)
    # nants = 4
    # colors = ['k', 'g', '#999fff', 'y', 'r']

    # for ix,samples in enumerate(samples_range):
    #     for iy,nants in enumerate(nant_range):
    #         dstr = 'plain'+str(samples)+str(nants)
    #         plt.plot(bins, stats[dstr][0].get_cdf(bins), linewidth=4, label='Nant=%d,Samp=%d'%(nants, samples), color=colors[ix])

    # # ax.set_yticks(np.arange(0,6,1))
    # plt.xlabel('Duration (us)')
    # plt.ylabel('CDF')
    # plt.ylim(0,1)
    # nospines(ax)
    # plt.grid()
    # pp = PdfPages('plot/cdf_trans_dur_ant_samp.pdf')
    # pp.savefig( bbox_inches='tight', dpi=300)
    # pp.close()
    # plt.close(fig)


