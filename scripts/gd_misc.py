from gd_stats import main, util_analysis, time_analysis
from gd_plot_config import plt, PdfPages, nospines
import numpy as np
import gd_utils as utils

mcs_range = [5, 10, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]




th_range = [4, 7, 14, 15, 16, 18, 19, 21, 22, 25, 27, 28, 30, 31]

fig = plt.figure(figsize=(4*2, 3*2))

res_us = utils.read_pickle('../dump/res_us_4.pkl');

cpu_u_orig, cpu_u, dead_miss_orig, dead_miss = [], [], [], []
for res_u in res_us:
    cpu_u_orig.append(sum(res_u[0]))
    cpu_u.append(sum(res_u[1]))
    dead_miss_orig.append(np.mean(res_u[2]))
    dead_miss.append(np.mean(res_u[3]))


ax = plt.gca()
ax1 = plt.subplot(211)
plt.plot(th_range, np.array(cpu_u_orig)*1.0/3, label='CPU Utilization', linewidth=4 )
nospines(ax1)
plt.ylabel('Percentage (%)')
plt.grid()
lg=ax1.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)
plt.ylim(0,60)
plt.yticks(range(0,80,20))
plt.setp(ax1.get_xticklabels(), visible=False)


ax2 = plt.subplot(212)
plt.plot(th_range, np.array(dead_miss_orig)*1.0/100, '-', label='Deadline miss', linewidth=4 , color='g')
plt.grid()

ax2.set_yscale('log')
plt.xlabel('Load (Mbps)')
plt.ylabel('Fraction')
plt.ylim(0.0001,1)
nospines(ax2)
# plt.title('CPU Utilization')

lg=ax2.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)

plt.grid()
pp = PdfPages('../plot/first_cpu_util_miss.pdf')
pp.savefig(bbox_inches='tight',  dpi=300)
pp.close()
plt.close(fig)




## plot offloading gains for 5/10 Mhz, 4/8 antennas

fig = plt.figure(figsize=(4*1.5, 3*1.5))
ax = plt.gca()
plt.plot(th_range, np.array(dead_miss_orig)*1.0/100, '-', label='Fixed', linewidth=4)
plt.plot(th_range, np.array(dead_miss)*1.0/100, '-', label='RT-OPEX', linewidth=4)

plt.grid()
ax.set_yscale('log')
plt.xlabel('Load (Mbps)')
# plt.xlabel('MCS')
plt.ylabel('Deadline miss')
plt.ylim(0.0001,1)
nospines(ax)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)

plt.grid()
pp = PdfPages('../plot/4_cpu_util_miss.pdf')
pp.savefig(bbox_inches='tight',  dpi=300)
pp.close()
plt.close(fig)



for ant in [4,6,8]:

    res_us = utils.read_pickle('../dump/res_us_%d.pkl'%ant);

    cpu_u_orig, cpu_u, dead_miss_orig, dead_miss = [], [], [], []
    for res_u in res_us:
        cpu_u_orig.append(sum(res_u[0]))
        cpu_u.append(sum(res_u[1]))
        dead_miss_orig.append(np.mean(res_u[2]))
        dead_miss.append(np.mean(res_u[3]))


    fig = plt.figure(figsize=(4*1.5, 3*1.5))
    ax = plt.gca()
    plt.plot(th_range, np.array(dead_miss_orig)*1.0/100, '-', label='Fixed', linewidth=4)
    plt.plot(th_range, np.array(dead_miss)*1.0/100, '-', label='RT-OPEX', linewidth=4)

    plt.grid()
    ax.set_yscale('log')
    plt.xlabel('Load (Mbps)')
    # plt.xlabel('MCS')
    plt.ylabel('Deadline miss')
    plt.ylim(0.0001,1)
    nospines(ax)

    lg=ax.legend(loc='upper left',numpoints=1)
    if lg is not None:
        lg.draw_frame(False)

    plt.grid()
    pp = PdfPages('../plot/%d_cpu_util_miss.pdf'%ant)
    pp.savefig(bbox_inches='tight',  dpi=300)
    pp.close()
    plt.close(fig)






# RTT time
nant_range = [2,4,6,8, 10, 12, 14]

arrts, trans_m, trans_err = [], [], []
for nant in nant_range:
    arrt = utils.read_pickle('../dump/trans_%d.pkl'%nant)
    arrts.append(arrt)
    trans_m.append(np.mean(arrt))
    trans_err.append(np.std(arrt))

fig = plt.figure(figsize=(4*1.5, 3*1.5))
ax = plt.gca()
plt.errorbar (nant_range, trans_m, yerr=trans_err, label='GPP', linewidth=4)
plt.errorbar (nant_range, trans_m, yerr=trans_err, label='KVM', linewidth=4)

plt.grid()
plt.xlabel('# Antennas')
plt.xlim([1,15])
plt.ylim([800, 1100])
plt.ylabel('RTT/2 (us)')
nospines(ax)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)

plt.grid()
pp = PdfPages('../plot/rtt_ant.pdf')
pp.savefig(bbox_inches='tight',  dpi=300)
pp.close()
plt.close(fig)


















# maximum system capacity for 10**-3 dead miss

cap = [31, 15, 6.1, 0.8]
capo = [31, 22, 17, 14.8]

fig = plt.figure(figsize=(4*1.5, 3*1.5))
ax = plt.gca()
N = 4
ind = np.arange(N)+0.2  # the x locations for the groups
width = 0.2
rects1 = ax.bar(ind, cap, width, color = '#999fff', alpha=1, capsize=8, linewidth= 1, label='Fixed')
rects2 = ax.bar(ind+0.2, capo, width, color = 'black', alpha=1, capsize=8, linewidth= 1, label='RT-OPEX')


plt.ylabel('Capacity (Mbps)')
plt.xticks(ind+0.1)
ax.set_xticklabels( ('N=2', 'N=4', 'N=6', 'N=8') )
plt.ylim([0, 35])
nospines(ax)
ax.yaxis.grid(True)

lg=ax.legend(loc='upper right',numpoints=1)
if lg is not None:
    lg.draw_frame(False)



pp = PdfPages('../plot/capacity_miss.pdf')
pp.savefig( bbox_inches='tight', dpi=300)
pp.close()
plt.close(fig)















# compare against cloudiq













