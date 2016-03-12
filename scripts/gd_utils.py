#! /usr/bin/python

from multiprocessing import Pool, cpu_count
import cPickle as pickle
import datetime
import sys, pdb, os
import numpy as np

class runtimetracker(object):
  timecalc0 = None
  reporting = True
  tick_message = ''

  def tick(self, message=''):
    self.timecalc0 = datetime.datetime.now()
    if self.reporting:
      self.tick_message = message
      if len(message)>0:
        print message,
        sys.stdout.flush()

  def tock_sec(self):
    return (datetime.datetime.now() - self.timecalc0).seconds

  def tock(self, message=''):
    if self.reporting:
      if len(message)==0:
        # print ' Runtime (hh:mm:ss.ssss) = ', datetime.datetime.now() - self.timecalc0
        if len(self.tick_message)==0:
          print 'Runtime (hh:mm:ss.ssss) = ', datetime.datetime.now() - self.timecalc0
        else:
          print 'DONE <<%s>>, Runtime (hh:mm:ss.ssss) = ' % self.tick_message, datetime.datetime.now() - self.timecalc0
      else:
        print 'Runtime of %s (hh:mm:ss.ssss) = ' % message, datetime.datetime.now() - self.timecalc0
      sys.stdout.flush()

  def tocktick(self, message=''):
    if self.timecalc0 is not None:
      self.tock()
    self.tick(message)


def parallelize(fun, pars, *args):
    '''
    fun : function
    args: function arguments
    pars: parallel loop indices
    '''

    timer = runtimetracker()
    tmplist_tomerge = [None]*len(pars)
    timer.tick('parallel evaluation across %d parameters' % len(pars))
    pool = Pool(processes=cpu_count()+1) # general heuristic so that if a process is using IO, the rest cpu_count() ones can use all the CPUs
    for j in xrange(len(pars)):
        _args = (j,) + args
        # tmplist_tomerge[j] = pool.apply_async(fun, args= _args)
        tmplist_tomerge[j] = pool.apply_async(fun, (j,) + args)
    # re = pool.map_async(fun, pars)
    # re.get()
    pool.close()
    pool.join()
    timer.tock()

    timer.tocktick('merging parellel results')

    results = {} # dict
    for i, _ in enumerate(pars):
      results[i] = tmplist_tomerge[i].get()
    timer.tock()
    return results

def get_filesize_MB(fname):
  return os.path.getsize(fname)/(1024.*1024)

def write_pickle(obj, fname):
  fo = open(fname, 'wb')
  pickle.dump(obj, fo, protocol=pickle.HIGHEST_PROTOCOL)
  fo.close()

def read_pickle(fname):
  if os.path.isfile(fname):
    print 'File %s already exists.. Reading %.1f MB from disk..' % (fname, get_filesize_MB(fname))
    return pickle.load(open(fname, 'rb'))
  else:
    return None


def pdf(x,bins):
  assert(max(bins) >= max(x) and min(bins) <= min(x))
  a = np.histogram(np.array(x), bins=np.array(bins), density=True)
  return a[0]*np.diff(a[1])


def pdftocdf(p, bins):
  def cdf(x):
    # assert (len(x) > 0)
    if isinstance(x,list) or isinstance(x, np.ndarray):
      ret = []
      for item in np.array(x):
        ret.append(sum(p[bins[:-1] <= item])) # because len(bins) =  len(pdf) + 1
      return np.array(ret)

    else:
      return sum(p[bins[:-1] <= x])

  return cdf
