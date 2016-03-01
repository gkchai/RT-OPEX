# cloudIQ simulator
import os
import subprocess
from gd_stats import main, util_analysis, time_analysis
from gd_plot_config import plt, PdfPages, nospines
import gd_utils as utils
import numpy as np


N = 4
M = 3
L = 4.0/3
T = 10000

core1 = np.zeros(T)
core2 = np.zeros(T)
core3 = np.zeros(T)
core4 = np.zeros(T)


def time(snr_0):
    snr =  np.random.exponential(snr_0)
    if snr > 25:
        turbo_iter = 1
    elif snr > 22.5 and snr < 25:
        turbo_iter = 2
    elif snr > 20 and snr < 22.5:
        turbo_iter = 3
    else:
        turbo_iter = 4

    return (3.77428571*96.8*turbo_iter + 43.9*6 + 93.0*4 + 28.4)/1000



ciq, ours = [], []
for snr_0 in [10, 10**(1.5), 100, 10**(2.5), 1000]:

    d_m_r_o, d_m_r = [], []
    # every t calculate available offload
    for tt in range(0, T):
        t_p = time(snr_0)
        t = time(snr_0)
        # off_avail = max(min(0.33, 1.33 - t_p),0)

        if t_p > 1.33:
            off_avail = 0
        elif t_p < 1.33 and t_p > 1:
            off_avail = 1.33 - t_p
        else:
            off_avail = 0.33


        if t > 2.0:
            deadline_miss_rate_orig = 1
        else:
            deadline_miss_rate_orig = 0

        # print t, off_avail, t_p

        if t - off_avail > 2.0:
            deadline_miss_rate = 1
        else:
            deadline_miss_rate = 0


        d_m_r_o.append(deadline_miss_rate_orig*1.0/3)
        d_m_r.append(deadline_miss_rate*1.0/3)


    print "Miss rate original = %f, ours = %f"%(np.mean(d_m_r_o), np.mean(d_m_r))
    ciq.append(np.mean(d_m_r_o))
    ours.append(np.mean(d_m_r))




color1 = [68.0/255,78.0/255,154.0/255]
color2 = [149.0/255,55.0/255,53.0/255]

fig = plt.figure(figsize=(4*1.7, 3*1.7))
ax = plt.gca()
N = 5
ind = np.arange(N)+0.1  # the x locations for the groups
width = 0.15

ans = []
for i in range(3):
    ans.append(100*(1- ours[i]/ciq[i]) )

rectso = ax.bar(ind, ciq, width, color = 'white', alpha=1, capsize=8, linewidth= 1, label='CloudIQ')
rects1 = ax.bar(ind+0.15, ours, width, color = color2, alpha=1, capsize=8, linewidth= 1, label='RT-OPEX')

plt.ylabel('Deadline miss')
plt.xticks(ind+0.1)
ax.set_xticklabels( ('10dB', '15dB', '20dB', '25dB', '30dB') )
plt.xlabel('SNR')
ax.set_yscale('log')
# ax.set_yscale('log')
plt.ylim([0.0001, 1])
# plt.xlim([0, 1.6])

nospines(ax)
ax.yaxis.grid(True)

# lg=ax.legend(loc='upper left',numpoints=1)
# if lg is not None:
#     lg.draw_frame(False)


pp = PdfPages('../plot/ciq_comp.pdf')
pp.savefig( bbox_inches='tight', dpi=300)
pp.close()
plt.close(fig)



