from gd_plot_config import plt, PdfPages, nospines
import gd_utils as utils
import numpy as np

if __name__ == '__main__':

    from gd_stats import gstat, astat


    exp_range = ['plain', 'offload']
    samples_range = [1000]
    prior_range = [99]
    # sched_range = ['SCHED_FIFO', 'SCHED_RR', 'SCHED_OTHER']
    sched_range = ['SCHED_FIFO']
    nant_range = [4]
    nprocs = 3


    ##############################
    # slack cdf vs policy
    exp = 'plain'
    samples = 1000
    prior = 99
    sched_range = ['SCHED_FIFO', 'SCHED_RR', 'SCHED_OTHER']
    nants = 4

    stats = {}
    for sched in sched_range:
        obj = utils.read_pickle('../dump/gstat_exp%s_samp%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, prior,sched,nants, nprocs))
        stats[sched] = astat(obj.proc['slack'])


    fig = plt.figure(figsize=(4*1.5, 3*1.5))
    ax = plt.gca()
    bins = np.linspace(-500, 2000, 1000)
    plt.plot(bins, stats['SCHED_FIFO'].get_cdf(bins), linewidth=4, label='FIFO')
    plt.plot(bins, stats['SCHED_RR'].get_cdf(bins), linewidth=4, label='RR', color='#999fff')
    plt.plot(bins, stats['SCHED_OTHER'].get_cdf(bins), linewidth=4, label='OTHER', color='black')

    # ax.set_yticks(np.arange(0,6,1))
    plt.xlabel('Slack (us)')
    plt.ylabel('CDF')
    plt.ylim(0,1)
    nospines(ax)
    plt.grid()
    pp = PdfPages('plot/cdf_slack_sched.pdf')
    pp.savefig( bbox_inches='tight', dpi=300)
    pp.close()
    plt.close(fig)


    ##############################
    # cdf duration of transport, processing, offload
    prior = 99
    sched = 'SCHED_FIFO'
    nants = 4

    stats = {}
    for exp in exp_range:
        for samples in samples_range:
            for nants in nant_range:

                obj=utils.read_pickle('../dump/gstat_exp%s_samp%d_prior%d_sched%s_nant%d_nproc%d'%(exp, samples, prior,sched,nants, nprocs))
                stats[exp+str(samples)+str(nants)]=[astat(obj.trans['duration']), astat(obj.proc['duration']), astat(obj.offload['duration'])]

    # proc duration vs exp
    fig = plt.figure(figsize=(4*1.5, 3*1.5))
    ax = plt.gca()
    bins = np.linspace(0, 2000, 1000)
    samples = 1000
    nants = 4
    dstr = 'plain'+str(samples)+str(nants)
    plt.plot(bins, stats[dstr][1].get_cdf(bins), linewidth=4, label='FIFO')
    dstr = 'offload'+str(samples)+str(nants)
    plt.plot(bins, stats[dstr][1].get_cdf(bins), linewidth=4, label='RR', color='#999fff')

    # ax.set_yticks(np.arange(0,6,1))
    plt.xlabel('Duration (us)')
    plt.ylabel('CDF')
    plt.ylim(0,1)
    nospines(ax)
    plt.grid()
    pp = PdfPages('plot/cdf_proc_dur_exp.pdf')
    pp.savefig( bbox_inches='tight', dpi=300)
    pp.close()
    plt.close(fig)

    # trans duration vs samples, nants
    fig = plt.figure(figsize=(4*1.5, 3*1.5))
    ax = plt.gca()
    bins = np.linspace(0, 2000, 1000)
    nants = 4
    colors = ['k', 'g', '#999fff', 'y', 'r']

    for ix,samples in enumerate(samples_range):
        for iy,nants in enumerate(nant_range):
            dstr = 'plain'+str(samples)+str(nants)
            plt.plot(bins, stats[dstr][0].get_cdf(bins), linewidth=4, label='Nant=%d,Samp=%d'%(nants, samples), color=colors[ix])

    # ax.set_yticks(np.arange(0,6,1))
    plt.xlabel('Duration (us)')
    plt.ylabel('CDF')
    plt.ylim(0,1)
    nospines(ax)
    plt.grid()
    pp = PdfPages('plot/cdf_trans_dur_ant_samp.pdf')
    pp.savefig( bbox_inches='tight', dpi=300)
    pp.close()
    plt.close(fig)

