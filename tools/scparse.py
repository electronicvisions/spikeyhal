# # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# This file contains a python SpikeyConfig class to parse
# the entire chip configuration from the file
# spikeyconfig.out which is generated upon experiment
# execution.
#
# Author: Johannes Bill
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

import numpy as np

# # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # HARDWARE PARAMETERS # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # #

nBlock = 2
nDrvPerBlock = 256
nNeuronPerBlock = 192
nBiasb = 15
nOutamp = 9
nVout = 25
nProbepad = 1

# # # # # # # # # # # # # # # # # # # # # # # #
# # C O R E   D A T A   S T R U C T U R E S # #
# # # # # # # # # # # # # # # # # # # # # # # #

class Neuron(object):
  def __init__(self, id):
    self.id = int(id)
    self.block = self.id / nNeuronPerBlock
    self.blockid = self.id % nNeuronPerBlock
    self.initialized = False
    self.recspike = False
    self.recmembrane = False

  def set_config(self, bitstring):
    bitstring = bitstring.strip()
    assert len(bitstring) == 4, "Bitstring of neuron %d must have len 4." % self.id
    bs = np.array([int(b) for b in bitstring])[::-1]       # reversed: idx de facto from right to left
    if bs[1] == 1: # record membrane potential
      self.recmembrane = True
    if bs[2] == 1: # record spikes
      self.recspike = True
    self.initialized = True


class Driver(object):
  def __init__(self, id):
    self.id = int(id)
    self.block = self.id / nDrvPerBlock
    self.blockid = self.id % nDrvPerBlock
    self.initialized = False
    self.srctype = None         # "ext", "block0", "block1"
    self.srcid = None           # None for ext, int otherwise
    self.stp = None             # "static", "dep", "fac"
    self.excinh = None          # "exc", "inh"
    self.C2 = None        # STP change per AP in units of C1 / 8

  def set_config(self, bitstring):
    bitstring = bitstring.strip()
    assert len(bitstring) == 8, "Bitstring of drv %d must have len 8." % self.id
    # C4 C2  STPMODE       STPONOFF     INH  EXC  SAMEOTHER        RECEXT
    # -  -   0:fac 1:dep   0:off 1:on   0/1  0/1  0:other 1:same   0:ext 1:rec
    bs = np.array([int(b) for b in bitstring])[::-1]       # reversed: idx de facto from right to left
    if bs[0] == 0:  # external
      self.srctype = "ext"
    elif bs[0] == 1:  # recurrent
      if bs[1] == 1:  # this block
        self.srctype = "block" + str(self.block)
        self.srcid = (self.block % 2) * nNeuronPerBlock +  self.blockid
      elif bs[1] == 0: # other block
        self.srctype = "block" + str((self.block + 1) % nBlock)
        self.srcid = ((self.block + 1) % 2) * nNeuronPerBlock + self.blockid + (2 * ( (1 + self.blockid) % 2 ) - 1)   # drv lines are twisted across chip halves!
    if bs[2] == 1: # exc
      self.excinh = "exc"
      assert bs[3] == 0, "Error: Driver %d is both exc and inh." % self.id
    if bs[3] == 1: # inh
      self.excinh = "inh"
      assert bs[2] == 0, "Error: Driver %d is both exc and inh." % self.id
    if bs[4] == 1: # stp
      if bs[5] == 0: # fac
        self.stp = 'fac'
      else:
        self.stp = 'dep'
    else:
      self.stp = 'static'
    self.C2 = 1 + 2 * bs[6] + 4 * bs[7]
    self.initialized = True



# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # P A R S E   A N D   L O A D   F U N C T I O N S # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

def load_chip_param(X, CP, VERBOSE):
  if VERBOSE: print " > Loading global chip parameters."
  for ln,x in enumerate(X):
    if "ud_chip" in x:
      if VERBOSE: print " > Start of chip parameters detected at line %d." % (ln+2)
      break
  _,tsense,_,tpcsec,_,tpcorperiod = X[ln+1].strip().split(' ')
  CP['tsense'] = tsense
  CP['tpcsec'] = tpcsec
  CP['tpcorperiod'] = tpcorperiod
  if VERBOSE: print " > Global chip parameters loaded."


def load_dac_param(X, DP, VERBOSE):
  if VERBOSE: print " > Loading DAC parameters."
  for ln,x in enumerate(X):
    if "ud_dac" in x:
      if VERBOSE: print " > Start of DAC parameters detected at line %d." % (ln+2)
      break
  _,irefdac,_,vcasdac,_,vm,_,vstart,_,vrest = X[ln+1].strip().split(' ')
  DP['irefdac'] = irefdac
  DP['vcasdac'] = vcasdac
  DP['vm'] = vm
  DP['vstart'] = vstart
  DP['vrest'] = vrest
  if VERBOSE: print " > DAC parameters loaded."


def load_analog_param(X, AP, VERBOSE):
  AP['drv'] = dict()
  AP['nrn'] = dict()
  AP['biasb'] = np.zeros(nBiasb, dtype=object)
  AP['outamp'] = np.zeros(nOutamp, dtype=object)
  AP['vout'] = np.zeros((nBlock, nVout), dtype=object)
  AP['voutbias'] = np.zeros((nBlock, nVout), dtype=object)
  AP['probepad'] = np.zeros((nBlock, nProbepad), dtype=object)
  AP['probebias'] = np.zeros((nBlock, nProbepad), dtype=object)
  if VERBOSE: print " > Loading analog parameters."
  # Synapse params
  for ln,x in enumerate(X):
    if "ud_param" in x:
      if VERBOSE: print " > Start of analog parameters detected at line %d." % (ln+2)
      break
  for i in xrange(nBlock * nDrvPerBlock):
    try: # test next line
      _,sid,_,drviout,_,adjdel,_,drvifall,_,drvirise= X[ln+1].strip().split(' ')
      if int(sid) == i: # next line holds config
        ln += 1 # set next line as current
    except:
      pass # nothing to be done
    _,sid,_,drviout,_,adjdel,_,drvifall,_,drvirise= X[ln].strip().split(' ')
    AP['drv'][i] = dict(drviout=drviout, adjdel=adjdel, drvifall=drvifall, drvirise=drvirise)
    if VERBOSE: print '   > Drv %d' % i, AP['drv'][i]
  # Neuron params
  ln += 2
  for i in xrange(nBlock * nNeuronPerBlock):
    try: # test next line
      _,nid,_,ileak,_,icb = X[ln+1].strip().split(' ')
      if int(nid) == i: # next line holds config
        ln += 1 # set next line as current
    except:
      pass # nothing to be done
    _,nid,_,ileak,_,icb = X[ln].strip().split(' ')
    AP['nrn'][i] = dict(ileak=ileak, icb=icb)
    if VERBOSE: print '   > Nrn %d' % i, AP['nrn'][i]
  # biasb
  ln += 2
  assert X[ln].strip().split(' ')[0] == "biasb", "Wrong start of line %d! Expected: biasb" % (ln+1)
  for i, bb in enumerate(X[ln].strip().split(' ')[1:]):
      AP['biasb'][i] = bb
  # Outamp
  ln += 1
  assert X[ln].strip().split(' ')[0] == "outamp", "Wrong start of line %d! Expected: outamp" % (ln+1)
  for i, oa in enumerate(X[ln].strip().split(' ')[1:]):
      AP['outamp'][i] = oa
  # Vout
  ln += 1
  assert X[ln].strip().split(' ')[0] == "vout", "Wrong start of line %d! Expected: vout" % (ln+1)
  for b in xrange(nBlock):
    for i, v in enumerate(X[ln].strip().split()[int(b==0):]):
      AP['vout'][b,i] = v
    ln += 1
  # Voutbias
  assert X[ln].strip().split(' ')[0] == "voutbias", "Wrong start of line %d! Expected: voutbias" % (ln+1)
  for b in xrange(nBlock):
    for i, v in enumerate(X[ln].strip().split()[int(b==0):]):
      AP['voutbias'][b,i] = v
    ln += 1
  # probepad
  assert X[ln].strip().split(' ')[0] == "probepad", "Wrong start of line %d! Expected: probepad" % (ln+1)
  for b in xrange(nBlock):
    for i, v in enumerate(X[ln].strip().split()[int(b==0):]):
      AP['probepad'][b,i] = v
    ln += 1
  # probebias
  assert X[ln].strip().split(' ')[0] == "probebias", "Wrong start of line %d! Expected: probebias" % (ln+1)
  for b in xrange(nBlock):
    for i, v in enumerate(X[ln].strip().split()[int(b==0):]):
      AP['probebias'][b,i] = v
    ln += 1
  if VERBOSE: print " > Analog parameters loaded."


def load_neuron_config(X, neuron, VERBOSE):
  if VERBOSE: print " > Loading neuron configuration."
  for ln,x in enumerate(X):
    if "ud_colconfig" in x:
      if VERBOSE: print " > Start of neuron config detected at line %d." % (ln+2)
      break
  for i in xrange(nBlock * nNeuronPerBlock):
    try: # test next line
      _,nid,_,bitstring = X[ln+1].strip().split(' ')
      if int(nid) == i: # next line holds config
        ln += 1 # set next line as current
    except:
      pass # nothing to be done
    _,nid,_,bitstring = X[ln].strip().split(' ')
    if VERBOSE: print "   > Neuron %3d set to '%s' (from line %d)." % (i, bitstring, ln+1)
    nrn = Neuron(i)
    nrn.set_config(bitstring)
    neuron[nrn.block, nrn.blockid] = nrn
  if VERBOSE: print " > Neuron configuration loaded."


def load_driver_config(X, driver, VERBOSE):
  if VERBOSE: print " > Loading synapse driver configuration."
  for ln,x in enumerate(X):
    if "ud_rowconfig" in x:
      if VERBOSE: print " > Start of syndriver config detected at line %d." % (ln+2)
      break
  for i in xrange(nBlock * nDrvPerBlock):
    try: # test next line
      _,sid,_,bitstring = X[ln+1].strip().split(' ')
      if int(sid) == i: # next line holds config
        ln += 1 # set next line as current
    except:
      pass # nothing to be done
    _,sid,_,bitstring = X[ln].strip().split(' ')
    if VERBOSE: print "   > Driver %3d set to '%s' (from line %d)." % (i, bitstring, ln+1)
    drv = Driver(i)
    drv.set_config(bitstring)
    driver[drv.block, drv.blockid] = drv
  if VERBOSE: print " > Synapse driver configuration loaded."


def load_weight_matrix(X, W, driver, VERBOSE):
  for ln,x in enumerate(X):
    if "ud_weight" in x:
      if VERBOSE: print " > Start of weight matrix detected at line %d." % (ln+2)
      ln += 1  # afterwards ln points to first line with syndrv config
      break
  for i in xrange(nBlock*nNeuronPerBlock):
    x = X[ln+i].strip()
    assert i == int(x[2:5])
    W[i] = [int(w, 16) for w in x[6:].split(' ')]
  W0 = np.zeros((nNeuronPerBlock, nDrvPerBlock), dtype=int)
  W1 = np.zeros((nNeuronPerBlock, nDrvPerBlock), dtype=int)
  sdict = dict(exc=+1, inh=-1)
  for i,((drv0,drv1),w) in enumerate(zip(driver.T, W.T)):
    s0, s1 = sdict[drv0.excinh], sdict[drv1.excinh]
    W0[:,i] = s0 * w[:nNeuronPerBlock]
    W1[:,i] = s1 * w[nNeuronPerBlock:]
  if VERBOSE: print " > Weight matrix loaded."
  return W0, W1



# # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # SPIKEYCONFIG  CLASS # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # #

class SpikeyConfig(object):
    def __init__(self, srcfile, VERBOSE=False):
        self.srcfile = srcfile # spikeyconfig.out file name
        self.VERBOSE = VERBOSE
        self.CP = dict()    # Chip parameters
        self.DP = dict()    # DAC parameters
        self.AP = dict()    # analog parameters
        self.neuron = np.zeros((nBlock, nNeuronPerBlock), dtype=object) # neurons (structural config)
        self.driver = np.zeros((nBlock, nDrvPerBlock), dtype=object)    # synapses (structural config)
        self.W = np.zeros((nBlock*nNeuronPerBlock, nDrvPerBlock), dtype=int) # full weight matrix
        self.W0, self.W1 = None, None   # separate synapse arrays with signed weight values for exc / inh syns
        # load config
        self.load_sc()

    def load_sc(self):
        if self.VERBOSE: print " > Load Spikey configuration from '%s'." % self.srcfile
        f = open(self.srcfile)
        X = f.readlines()
        load_chip_param(X, self.CP, self.VERBOSE)
        load_dac_param(X, self.DP, self.VERBOSE)
        load_analog_param(X, self.AP, self.VERBOSE)
        load_neuron_config(X, self.neuron, self.VERBOSE)
        load_driver_config(X, self.driver, self.VERBOSE)
        self.W0, self.W1 = load_weight_matrix(X, self.W, self.driver, self.VERBOSE)
        f.close()
        if self.VERBOSE: print " > Spikey configuration loaded."


# # # # # # # # # # # # # # # # # # # # # # # #
# # #   J U S T   F O R   T E S T I N G # # # #
# # # # # # # # # # # # # # # # # # # # # # # #

if __name__ == '__main__':
    fname = "spikeyconfig_schmuker.out"
    sc = SpikeyConfig(fname, VERBOSE=True)


