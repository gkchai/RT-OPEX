import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import rcParams
from matplotlib.backends.backend_pdf import PdfPages


params = {
   'axes.labelsize': 26,
   'legend.fontsize': 25,
   'xtick.labelsize': 23,
   'ytick.labelsize': 23,
   'text.usetex': False,
   'figure.figsize': [4, 3],
    # 'figure.figsize': [18, 12],
    # 'font.family': 'Helvetica',
    'font.family': 'sans-serif',
    'font.sans-serif': 'Helvetica',
    # 'pdf.fonttype': 42,
    # 'ps.usedistiller': 'xpdf'
    # 'xtick.major.pad': 10,
    # 'ytick.major.pad': 10,
    'xtick.major.size': 8,
    'xtick.major.width':1,
    'xtick.direction': 'out',
    'ytick.major.size': 8,
    'ytick.major.width':1,
    'ytick.direction': 'out',

   }
rcParams.update(params)



fmts = ['-o', '--o', '-s', '--s', '-x', '--x', '-v', '--v']
ls = ['-', '--', '-', '--', '-', '--', '-', '--']
ms = ['o', 'o', 's', 's', 'v', 'v', 'v', 'v']
colors = ['b', 'g', 'r', 'y']


'''
best -- 0
upper right -- 1
upper left -- 2
lower left -- 3
lower right -- 4
right -- 5
center left -- 6
center right -- 7
lower center -- 8
upper center -- 9
center -- 10
'''


def nospines(ax):

    lg=ax.legend(loc='best',numpoints=1)
    if lg is not None:
        lg.draw_frame(False)

    ax.spines['right'].set_visible(False)
    ax.yaxis.set_ticks_position('left')
    ax.spines['top'].set_visible(False)
    ax.xaxis.set_ticks_position('bottom')
    ax.grid(False)
