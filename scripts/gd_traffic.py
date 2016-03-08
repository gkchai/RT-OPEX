#!/usr/bin/python

# generate mcs trace from
# traffic data

import numpy as np
import gd_utils as utils
import os.path
from pprint import pprint
import pdb

filename = "traffic.txt"

arr = np.loadtxt(filename)

max_val = max(arr)
for i in range(len(arr)):
    arr[i] = int((arr[i]/max_val)*27)


v_50 = np.percentile(arr,50)
v_75 = np.percentile(arr,75)
v_95 = np.percentile(arr,95)
v_99 = np.percentile(arr,99)

print "75th percentile MCS is %d"%v_75
print "95th percentile MCS is %d"%v_95
print "99th percentile MCS is %d"%v_99

out_arr = []
for i in range(1000):
    out_arr.extend(arr)


np.savetxt('../src/mcs.txt', out_arr, fmt = '%d')





