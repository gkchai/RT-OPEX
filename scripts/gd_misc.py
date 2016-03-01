from gd_stats import main, util_analysis, time_analysis
from gd_plot_config import plt, PdfPages, nospines
import numpy as np
import gd_utils as utils

mcs_range = [5, 10, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]


color1 = [68.0/255,78.0/255,154.0/255]
color2 = [149.0/255,55.0/255,53.0/255]

th_range = [4, 7, 14, 15, 16, 18, 19, 21, 22, 25, 27, 28, 30, 31]

# fig = plt.figure(figsize=(4*2, 3*2))

# res_us = utils.read_pickle('../dump/res_us_4.pkl');

# cpu_u_orig, cpu_u, dead_miss_orig, dead_miss = [], [], [], []
# for res_u in res_us:
#     cpu_u_orig.append(sum(res_u[0]))
#     cpu_u.append(sum(res_u[1]))
#     dead_miss_orig.append(np.mean(res_u[2]))
#     dead_miss.append(np.mean(res_u[3]))


# ax = plt.gca()
# ax1 = plt.subplot(211)
# plt.plot(th_range, np.array(cpu_u_orig)*1.0/3, label='CPU Utilization', linewidth=4 )
# nospines(ax1)
# plt.ylabel('Percentage (%)')
# plt.grid()
# lg=ax1.legend(loc='upper left',numpoints=1)
# if lg is not None:
#     lg.draw_frame(False)
# plt.ylim(0,60)
# plt.yticks(range(0,80,20))
# plt.setp(ax1.get_xticklabels(), visible=False)


# ax2 = plt.subplot(212)
# plt.plot(th_range, np.array(dead_miss_orig)*1.0/100, '-', label='Deadline miss', linewidth=4 , color='g')
# plt.grid()

# ax2.set_yscale('log')
# plt.xlabel('Load (Mbps)')
# plt.ylabel('Fraction')
# plt.ylim(0.0001,1)
# nospines(ax2)
# # plt.title('CPU Utilization')

# lg=ax2.legend(loc='upper left',numpoints=1)
# if lg is not None:
#     lg.draw_frame(False)

# plt.grid()
# pp = PdfPages('../plot/first_cpu_util_miss.pdf')
# pp.savefig(bbox_inches='tight',  dpi=300)
# pp.close()
# plt.close(fig)




## plot offloading gains for 5/10 Mhz, 4/8 antennas

# fig = plt.figure(figsize=(4*1.5, 3*1.5))
# ax = plt.gca()
# plt.plot(th_range, np.array(dead_miss_orig)*1.0/100, '-', label='Fixed', linewidth=4)
# plt.plot(th_range, np.array(dead_miss)*1.0/100, '-', label='RT-OPEX', linewidth=4)

# plt.grid()
# ax.set_yscale('log')
# plt.xlabel('Load (Mbps)')
# # plt.xlabel('MCS')
# plt.ylabel('Deadline miss')
# plt.ylim(0.0001,1)
# nospines(ax)

# lg=ax.legend(loc='upper left',numpoints=1)
# if lg is not None:
#     lg.draw_frame(False)

# plt.grid()
# pp = PdfPages('../plot/4_cpu_util_miss.pdf')
# pp.savefig(bbox_inches='tight',  dpi=300)
# pp.close()
# plt.close(fig)

# color1 = [68.0/255,78.0/255,154.0/255]
# color2 = [149.0/255,55.0/255,53.0/255]

# for ant in [4,6,8]:

#     res_us = utils.read_pickle('../dump/res_us_%d.pkl'%ant);

#     cpu_u_orig, cpu_u, dead_miss_orig, dead_miss = [], [], [], []
#     for res_u in res_us:
#         cpu_u_orig.append(sum(res_u[0]))
#         cpu_u.append(sum(res_u[1]))
#         dead_miss_orig.append(np.mean(res_u[2]))
#         dead_miss.append(np.mean(res_u[3]))


#     fig = plt.figure(figsize=(4*1.5, 3*1.5))
#     ax = plt.gca()
#     plt.plot(th_range, np.array(dead_miss_orig)*1.0/100, '-o', color=color1, mfc='none', label='Fixed', ms=14, mec=color1, linewidth=4, markeredgewidth=4)
#     plt.plot(th_range, np.array(dead_miss)*1.0/100, '-o', color=color2, mfc='none', label='RT-OPEX', ms=14, mec=color2, linewidth=4, markeredgewidth=4)

#     plt.grid()
#     ax.set_yscale('log')
#     plt.xlabel('Load (Mbps)')
#     # plt.xlabel('MCS')
#     plt.ylabel('Deadline miss')
#     plt.ylim(0.0001,1.2)
#     nospines(ax)

#     # lg=ax.legend(loc='upper left',numpoints=1)
#     # if lg is not None:
#     #     lg.draw_frame(False)

#     plt.grid()
#     pp = PdfPages('../plot/%d_cpu_util_miss.pdf'%ant)
#     pp.savefig(bbox_inches='tight',  dpi=300)
#     pp.close()
#     plt.close(fig)






# RTT time
nant_range = [2,4,6,8, 10, 12, 14]


trans_m = [908, 921, 932, 931, 929, 990, 998]
trans_err = [13, 15, 20, 17, 28, 39, 48]

trans_m_5 = [572, 577, 580, 594, 601, 627, 665,]
trans_err_5 = [12, 9, 13, 18, 12, 14, 31,]

trans_m_k = [961, 971, 984, 988, 1008, 1030, 1172]
trans_err_k = [49, 80, 74, 113, 159, 248, 443]

trans_m_k_5 = [620, 630, 643, 656, 700, 724, 797]
trans_err_k_5 = [36, 38, 60, 108, 198, 196, 324]


fig = plt.figure(figsize=(4*1.5, 3*1.5))
ax = plt.gca()
plt.errorbar (nant_range, trans_m, yerr=trans_err, linewidth=4, color='blue', label='10MHz')
plt.errorbar (nant_range, trans_m_5, yerr=trans_err_5, linewidth=4, color='orange', label='5MHz')

plt.grid()
plt.xlabel('# Antennas')
plt.xlim([1,15])
plt.ylim([400, 1400])
plt.ylabel('RTT/2 (us)')
nospines(ax)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)

plt.grid()
pp = PdfPages('../plot/rtt_ant_g.pdf')
pp.savefig(bbox_inches='tight',  dpi=300)
pp.close()
plt.close(fig)

fig = plt.figure(figsize=(4*1.5, 3*1.5))
ax = plt.gca()
plt.errorbar (nant_range, trans_m_k, yerr=trans_err_k, linewidth=4, color='black', label='10MHz')
plt.errorbar (nant_range, trans_m_k_5, yerr=trans_err_k_5, linewidth=4, color='gray', label='5MHz')

plt.grid()
plt.xlabel('# Antennas')
plt.xlim([1,15])
plt.ylim([400, 1400])
plt.ylabel('RTT/2 (us)')
nospines(ax)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)

plt.grid()
pp = PdfPages('../plot/rtt_ant_k.pdf')
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




# average offload

res_p, res_o = [], []
for ant in [2,4,6,8]:
    res = utils.read_pickle('../dump/proc_offload_%d.pkl'%ant)
    res_p.append(res[0][0])
    res_o.append(res[1][0])


orig_proc = [741, 927, 1113, 1299]

fig = plt.figure(figsize=(4*1.7, 3*1.7))
ax = plt.gca()
N = 4
ind = np.arange(N)+0.2  # the x locations for the groups
width = 0.15

rectso = ax.bar(ind, orig_proc, width, color = 'white', alpha=1, capsize=8, linewidth= 1, label='Original Proc')
rects1 = ax.bar(ind+0.15, res_p, width, color = 'darkgray', alpha=1, capsize=8, linewidth= 1, label='Local Proc')
rects2 = ax.bar(ind+0.3, res_o, width, color = 'black', alpha=1, capsize=8, linewidth= 1, label='Offload')


plt.ylabel('Avg. Time (us)')
plt.xticks(ind+0.1)
ax.set_xticklabels( ('N=2', 'N=4', 'N=6', 'N=8') )
plt.ylim([0, 1650])
nospines(ax)
ax.yaxis.grid(True)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)


pp = PdfPages('../plot/proc_offload_ant.pdf')
pp.savefig( bbox_inches='tight', dpi=300)
pp.close()
plt.close(fig)



# cpu utilization


cpu_u_orig, cpu_u, dead_miss_orig, dead_miss = [], [], [], []

for ant in [2,4,6,8]:
    res_us = utils.read_pickle('../dump/res_us_%d.pkl'%ant);

    res_u = res_us[3]
    cpu_u.append(sum(res_u[0])/3)
    cpu_u_orig.append(sum(res_u[1])/3)


fig = plt.figure(figsize=(4*1.5, 3*1.5))
ax = plt.gca()
N = 4
ind = np.arange(N)+0.1  # the x locations for the groups
width = 0.15

rectso = ax.bar(ind, cpu_u_orig, width, color = 'white', alpha=1, capsize=8, linewidth= 1, label='Fixed')
rects1 = ax.bar(ind+0.15, cpu_u, width, color = 'darkgray', alpha=1, capsize=8, linewidth= 1, label='RT-OPEX')

plt.ylabel('CPU Util (%)')
plt.xticks(ind+0.1)
ax.set_xticklabels( ('N=2', 'N=4', 'N=6', 'N=8') )
plt.ylim([0, 60])
nospines(ax)
ax.yaxis.grid(True)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)


pp = PdfPages('../plot/cpu_util_ant.pdf')
pp.savefig( bbox_inches='tight', dpi=300)
pp.close()
plt.close(fig)


# plot for offload delay


fig = plt.figure(figsize=(4*1.7, 3*1.7))
ax = plt.gca()
N = 2
ind = 0.6*np.arange(N)+0.2  # the x locations for the groups
width = 0.1
cost_offload = 2*np.array([26.3, 36.3])
c_o_err = 2*np.array([10, 15])

rectso = ax.bar(ind, cost_offload, width, color = 'seagreen', yerr=c_o_err, alpha=1, capsize=8, linewidth= 2, error_kw = {'ecolor':'black', 'elinewidth':1.5})

plt.ylabel('Offload overhead (ns)')
plt.xlabel('FFT')
plt.xticks(ind+0.05)
plt.xlim(0,1.2)
ax.set_xticklabels( ('512', '1024') )
plt.ylim([0, 120])
nospines(ax)
ax.yaxis.grid(True)

lg=ax.legend(loc='upper left',numpoints=1)
if lg is not None:
    lg.draw_frame(False)


pp = PdfPages('../plot/cost_offload.pdf')
pp.savefig( bbox_inches='tight', dpi=300)
pp.close()
plt.close(fig)


# compare against cloudiq











