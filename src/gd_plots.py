from gd_plot_config import plt, PdfPages, nospines
import gd_utils as utils
import numpy as np

if __name__ == '__main__':

    from gd_stats import gstat, astat

    prior = 99
    sched_range = ['SCHED_FIFO', 'SCHED_RR', 'SCHED_OTHER']
    nants = 4
    nprocs = 3

    objs = {}
    stats = {}
    for sched in sched_range:
        objs[sched] = utils.read_pickle('../dump/gstat_prior%d_sched%s_nant%d_nproc%d'%(prior,sched, nants, nprocs))
        stats[sched] = astat(objs[sched].proc['slack'])


    # slack cdf vs policy

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
