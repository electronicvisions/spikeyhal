#!/usr/bin/env python
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # #   S P I K E Y   C O N F I G   V I S U A L I Z A T I O N   # # #
# #                                                               # #
#        a small instructive helper script by Johannes Bill         #
# #                                                               # #
# # #                                                           # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

# # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # HARDWARE PARAMETERS # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # #

nBlock = 2
nDrvPerBlock = 256
nNeuronPerBlock = 192


# # # # # # # # # # # # # # # # # # # # # #
# # #  A R G U M E N T   P A R S E R  # # #
# # # # # # # # # # # # # # # # # # # # # #

import argparse

# command line argument parser
parser = argparse.ArgumentParser(description="Visualization of the Spikey chip's configuration.",
                     formatter_class=argparse.RawDescriptionHelpFormatter,
                     epilog="IPYTHON: In combination with ipython, you may have to add '--' for\n\
         correct argument parsing and select ipython's mpl backend, e.g.:\n\
         $ ipython -i -- scvisual.py -b GTKAgg [other argumens]\n ")
# input argument
parser.add_argument('-i', '--conf', metavar='FILE', default="spikeyconfig.out", dest="srcfile",
                    help="File to visualize (default: 'spikeyconfig.out')")
# output argument
parser.add_argument('-o', '--png', metavar='FILE', default=None, dest="pngfile",
                    help="File name for png image (default: Do not save output.)")
# resolution argument
parser.add_argument('-r', '--dpi', metavar='DPI', default=600, type=int, dest="DPI",
                    help="Resolution for saved image (default: 600)")
# backend argument
parser.add_argument('-b', '--backend', metavar='BACKEND', default="GTKAgg", dest="MPLBACKEND",
                    help="select matplotlib backend (default: GTKAgg)")
# verbose argument
parser.add_argument('-v', '--verbose', action='store_true', default=False, dest='VERBOSE',
                    help="Verbose output")
# version argument
parser.add_argument('--version', action='version', version="You're driving 'SCV 1'.")
args = parser.parse_args()

srcfile = args.srcfile
pngfile = args.pngfile          # png file name OR None (= do not save to file)
VERBOSE = args.VERBOSE
DPI = args.DPI
MPLBACKEND = args.MPLBACKEND
PLOT = True                     # True or False (= only parse spikeyconfig.out file)


# # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # COLOR DEFINITIONS # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # #

if VERBOSE: print " > Selected matplotlib backend: %s" % MPLBACKEND
import matplotlib
matplotlib.use(MPLBACKEND)

import numpy as np
import pylab as pl

namedColor = lambda cname: pl.matplotlib.colors.hex2color(pl.matplotlib.colors.cnames[cname])
# see: http://matplotlib.org/mpl_examples/color/named_colors.hires.png

# r, g, b, alpha
cExc = 1.0, 0.3, 0.3, 1.0
cInh = 0.4, 0.5, 1.0, 1.0
cExt = 0.2, 0.8, 0.2, 1.0
cBlock0 = namedColor('orchid')
cBlock1 = namedColor('turquoise')
cSpike = namedColor('orangered')
cMembrane = namedColor('deepskyblue')
cElectrode = namedColor('wheat')


# # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # MATPLOTLIB SETTINGS # # # # # #
# # # # # # # # # # # # # # # # # # # # # # #

mpl_config = {
    'axes' : dict(labelsize=8, titlesize=8, linewidth=0.5),
    'figure' : dict(dpi=86.4, facecolor='white'),
    'figure.subplot' : dict(left=0.15, bottom=0.15, right=0.97, top=0.97),
    'font' : {'family' : 'sans-serif', 'size' : 8, 'weight' : 'normal',
              'sans-serif' : ['Arial', 'LiberationSans-Regular', 'FreeSans']},
    'image' : dict(cmap='RdBu_r' , interpolation='nearest'),
    'legend' : dict(fontsize=8, borderaxespad=0.5, borderpad=0.5),
    'lines' : dict(linewidth=0.5),
    'xtick' : dict(labelsize=8),
    'xtick.major' : dict(size=2, pad=2),
    'ytick' : dict(labelsize=8),
    'ytick.major' : dict(size=2, pad=2),
    'savefig' : dict(dpi=DPI)
    }


# # # # # # # # # # # # # # # # # # # # # # #
# # # SUPPORTING PLOTTING FUNCTIONS # # # # #
# # # # # # # # # # # # # # # # # # # # # # #


def apply_mpl_settings(config):
  import matplotlib as mpl
  if VERBOSE: print " > Adjusting matplotlib defaults"
  for key,val in config.iteritems():
    s = ""
    for k,v in val.iteritems():
        s += k + "=%s, " % str(v)
    if VERBOSE: print "   > Set '%s' to %s" % (key, s[:-2])
    mpl.rc(key, **val)


def set_axis_grid(ax):
  # minor
  x = np.arange(0., nNeuronPerBlock+1, 16) - 0.5
  y = np.arange(0., nDrvPerBlock+1, 16) - 0.5
  for xi in x:
      ax.plot([xi,xi], [y[0],y[-1]],  lw=0.1, ls=':', c='k')
  for yi in y:
      ax.plot([x[0],x[-1]], [yi,yi],  lw=0.1, ls=':', c='k')
  # major
  x = np.arange(0., nNeuronPerBlock+1, 64) - 0.5
  y = np.arange(0., nDrvPerBlock+1, 64) - 0.5
  for xi in x:
      ax.plot([xi,xi], [y[0],y[-1]],  lw=0.3, ls='-', c='k')
  for yi in y:
      ax.plot([x[0],x[-1]], [yi,yi],  lw=0.3, ls='-', c='k')
  ax.set_xticks(np.arange(0, nNeuronPerBlock, 32))
  ax.set_yticks([])
  ax.xaxis.set_tick_params(size=0)
  ax.yaxis.set_tick_params(size=0)


def beautify_driver_axes(ax, ytickpos='left'):
  ax.set_xticks([])
  ax.set_yticks(np.arange(0, nDrvPerBlock, 32))
  ax.yaxis.set_tick_params(size=0)
  spines = ('top', 'bottom', 'left', 'right')
  for s in spines:
    ax.spines[s].set_visible(False)
  if ytickpos == 'left':
    ax.yaxis.tick_left()
  elif ytickpos == 'right':
    ax.yaxis.tick_right()


def beautify_neuron_axes(ax):
  ax.set_xticks(np.arange(0, nNeuronPerBlock, 32))
  ax.set_yticks([])
  ax.xaxis.set_tick_params(size=0)
  spines = ('top', 'bottom', 'left', 'right')
  ax.xaxis.tick_bottom()
  for s in spines:
    ax.spines[s].set_visible(False)


def generate_weight_cmap():
  clist = [pl.cm.Blues(0.8*i/15.+0.2) for i in np.arange(15,0,-1)]
  clist += [(1.,1.,1.,1.)]
  clist += [pl.cm.Reds(0.8*i/15.+0.2) for i in np.arange(1,16,1)]
  mycmap = pl.matplotlib.colors.ListedColormap(clist)
  return mycmap


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
    self.initialized = True


# # # # # # # # # # # # # # # # # # #
# # # CORE PLOTTING FUNCTIONS # # # #
# # # # # # # # # # # # # # # # # # #

def draw_neuron(nrn, ax, dy=0., escale=1., txtdx=0., forcefb=None):
  # (dy, escale, txtdx, forcefb are only for legend plotting)
  FancyBboxPatch = pl.matplotlib.patches.FancyBboxPatch
  y0 = -2.5 + dy
  if nrn.block == 0:
    x0 = float(nrn.blockid)
  elif nrn.block == 1:
    x0 = float(nrn.blockid)
  # neuron box
  fc = np.array({0 : cBlock0, 1 : cBlock1}[nrn.block])
  kwargs = dict(lw=0.1, ec='k', fc=fc, zorder=2, boxstyle="round,pad=0.2")
  p = FancyBboxPatch((x0-0.25, y0), 0.5, 2., **kwargs)
  ax.add_patch(p)
  # neuron index
  kwargs = dict(fontsize=1.3, rotation=90, va='center', ha='center', rasterized=True, zorder=3, clip_on=True)
  dx = {0 : +0.1, 1 : -0.1 }[nrn.block] + txtdx
  x, y = x0+dx, -1.3+dy
  ax.text(x, y, str(nrn.id), **kwargs)
  # small connector
  ax.plot((x0,x0), (0.+dy,-0.5+dy), 'k', lw=0.1, zorder=0)
  # if feeds back to network? --> small line
  if (nrn.id in [drv.srcid for drv in driver.flatten()] and forcefb!=False) or forcefb==True:
    ax.plot((x0,x0), (-2.5+dy,-4.0+dy), 'k', lw=0.1, zorder=0)
  # spikes recorded?
  if nrn.recspike:
    kwargs = dict(lw=0.0, ec=cSpike, fc=cSpike, zorder=1)
    p = pl.Circle((x0, y0-0.5), radius=0.3, **kwargs)
    ax.add_patch(p)
  # membrane recorded?
  if nrn.recmembrane:
    xy = np.array([(-0.05,0.), (-0.4, -3.), (0.,-3.), (0.,-0.5), (0.,-3.5), (0.,-3), (0.4,-3), (0.05, 0.)])
    xy *= 2.0 * escale
    kwargs = dict(closed=False, lw=0.1, ec='k', fc=cElectrode, zorder=4, clip_on=False)
    p = pl.Polygon(xy, **kwargs)
    trans = pl.matplotlib.transforms.Affine2D().rotate_deg(-25) + \
            pl.matplotlib.transforms.Affine2D().translate(x0, y0+0.1) + \
            ax.transData
    p.set_transform(trans)
    ax.add_patch(p)


def draw_driver(drv, ax):
  xy = np.array([(0.,0.), (-0.5, -0.45), (-1.25,-0.45), (-1.25,0.45), (-0.5,0.45)])
  # other block driver?
  if drv.block == 1:
    xy[:,0] *= -1
  xy[:,1] += drv.blockid
  fc = np.array(dict(exc=cExc, inh=cInh)[drv.excinh])
  kwargs = dict(closed=True, lw=0.1, ec='k', fc=fc, zorder=0)
  patch = pl.Polygon(xy, **kwargs)
  ax.add_patch(patch)
  # patch with src-color
  xy = np.array([(-1.25,-0.45), (-3., -0.45), (-3.,0.45), (-1.25,0.45)])
  if drv.block == 1:
    xy[:,0] *= -1
  xy[:,1] += drv.blockid
  fc = np.array(dict(ext=cExt, block0=cBlock0, block1=cBlock1)[drv.srctype])
  kwargs = dict(closed=True, lw=0.1, ec='k', fc=fc, zorder=1)
  patch = pl.Polygon(xy, **kwargs)
  ax.add_patch(patch)
  # small line for rec conn and srcid
  if drv.srctype != "ext":
    x = np.array([-3.5, -3])
    if  drv.block == 1:
      x *= -1
    ax.plot(x, [drv.blockid]*2, 'k', lw=0.1)
    kwargs = dict(fontsize=1.3, va='center', ha='center', rasterized=True, zorder=2, clip_on=True)
    x = -2.25
    if drv.block == 1:
      x = 2.25
    ax.text(x, drv.blockid, str(drv.srcid), **kwargs)
  # short term plasticity
  if drv.stp != "static":
    text = dict(fac="F", dep="D", static="S")[drv.stp]
    kwargs = dict(fontsize=1.5, va='center', ha='center', zorder=2, clip_on=True)
    x = -0.875
    if drv.block == 1:
      x *= -1
    ax.text(x, drv.blockid, text, **kwargs)


# # # # # # # # # # # # # # # # #
# # # D R A W   L E G E N D # # #
# # # # # # # # # # # # # # # # #

def draw_legend():
  ax = axes['LEG']
  txtkwargs = dict(x=1., fontsize=10., va='center', ha='left')
  # C4 C2  STPMODE       STPONOFF     INH  EXC  SAMEOTHER        RECEXT
  # -  -   0:fac 1:dep   0:off 1:on   0/1  0/1  0:other 1:same   0:ext 1:rec
  # drivers
  drv = Driver(2)
  drv.set_config('00000100')
  draw_driver(drv, ax)
  drv = Driver(1)
  drv.set_config('00110111')
  draw_driver(drv, ax)
  drv = Driver(0)
  drv.set_config('00011001')
  draw_driver(drv, ax)
  # resize text
  for ch in ax.get_children():
    if isinstance(ch, pl.Text):
      ch.set_fontsize(8.)
  # driver text
  ax.text(y=2., s="External input, excitatory, no short-term-plasticity", **txtkwargs)
  ax.text(y=1., s="Feedback from neuron 1, excitatory, depressing", **txtkwargs)
  ax.text(y=0., s="Feedback from neuron 193, inhibitory, facilitating ", **txtkwargs)
  # neurons
  ax = axes['LEGN']
  nrn = Neuron(28)
  nrn.set_config('0010')
  draw_neuron(nrn, ax, dy=3.5, escale=0.3, forcefb=False)
  nrn = Neuron(nNeuronPerBlock+41)
  nrn.set_config('0100')
  draw_neuron(nrn, ax, dy=3.5, txtdx=0.2, forcefb=True)
  # resize text
  for ch in ax.get_children():
    if isinstance(ch, pl.Text):
      ch.set_fontsize(8.)
  # Neuron text
  txtkwargs.pop('x', None)
  txtkwargs['y'] = 1.27
  ax.text(x=29.5, s="Neuron 28\nLeft block\nRecord voltage\nDoes not feed back", **txtkwargs)
  ax.text(x=42.5, s="Neuron 233\nRight block\nRecord spikes\nFeeds back to net", **txtkwargs)


# # # # # # # # # # # # # # # # # # # # # # # # #
# # # D A T A   L O A D   F U N C T I O N S # # #
# # # # # # # # # # # # # # # # # # # # # # # # #

def load_neuron_config(X, neuron):
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


def load_driver_config(X, driver):
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


def load_weight_matrix(X, W):
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


# # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # M A I N # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # #

apply_mpl_settings(mpl_config)

print " > Load Spikey configuration from '%s'." % srcfile

f = open(srcfile)
X = f.readlines()

# parse neuron config
neuron = np.zeros((nBlock, nNeuronPerBlock), dtype=object)
load_neuron_config(X, neuron)

# parse synapse driver config
driver = np.zeros((nBlock, nDrvPerBlock), dtype=object)
load_driver_config(X, driver)

# parse weight matrix
W = np.zeros((nBlock*nNeuronPerBlock, nDrvPerBlock), dtype=int)
W0, W1 = load_weight_matrix(X, W)

f.close()

if VERBOSE: print " > Spikey configuration loaded."


# plotting
if PLOT:
  pl.interactive(False)
  print " > Plotting... (this may take a moment)"
  # setup figure and axes
  w,h = 12., 7.5
  fig = pl.figure(figsize=(w,h))
  w,h = fig.get_figwidth(),fig.get_figheight()
  rfig = w/h
  w = 0.39
  h = w * 256./192. * rfig
  b = 0.16
  axes = dict()
  rect = np.array((0.10,0.02,0.80,0.08))
  axes['LEGS'] = fig.add_axes(rect+(0.007,-0.01,0,0), xlim=(-5., (80./8.)*4.*rfig - 5.), ylim=(-0.5,4-0.5),\
                             axis_bgcolor='0.85', xticks=[], yticks=[], aspect='equal')
  axes['LEG'] = fig.add_axes(rect, xlim=(-4.5, (80./8.)*3.6*rfig - 4.5), ylim=(-0.8,2.8),\
                             axis_bgcolor='0.95', xticks=[], yticks=[], aspect='equal')
  f = 1.35
  axes['LEGN'] = fig.add_axes(rect, xlim=(-5.5*f, (80./8.)*3.6*rfig*f - 5.5*f), ylim=(-0.8*f,2.8*f),\
                             frame_on=False, xticks=[], yticks=[], aspect='equal')
  rect = 0.10,b,w,h
  axes['WL'] = fig.add_axes(rect, xlim=(-0.5,nNeuronPerBlock-0.5), ylim=(-0.5,nDrvPerBlock-0.5), aspect='equal')
  rect = 0.90-w,b,w,h
  axes['WR'] = fig.add_axes(rect, xlim=(nNeuronPerBlock-0.5,-0.5), ylim=(-0.5,nDrvPerBlock-0.5), aspect='equal')
  wd = h * 3.7/256. / rfig
  rect = 0.10-wd,b,wd,h
  axes['DL'] = fig.add_axes(rect, xlim=(-3.6,0.1), ylim=(-0.5,nDrvPerBlock-0.5), aspect='equal', sharey=axes['WL'])
  rect = 0.90,b,wd,h
  axes['DR'] = fig.add_axes(rect, xlim=(-0.1,3.6), ylim=(-0.5,nDrvPerBlock-0.5), aspect='equal', sharey=axes['WR'])
  rect = 0.745,0.065,0.13,0.02
  axes['CB'] = fig.add_axes(rect)
  hn = w * 4.1/192. * rfig
  rect = 0.10,b-hn,w,hn
  axes['NL'] = fig.add_axes(rect, xlim=(-0.5,nNeuronPerBlock-0.5), ylim=(-4.1,0), aspect='equal', sharex=axes['WL'])
  rect = 0.90-w,b-hn,w,hn
  axes['NR'] = fig.add_axes(rect, xlim=(nNeuronPerBlock-0.5,-0.5), ylim=(-4.1,0), aspect='equal', sharex=axes['WR'])
  #
  if False:             # stop plotting during legend design
    draw_legend()
    import sys
    sys.exit()
  draw_legend()
  # plot weight matrix
  wcmap = generate_weight_cmap()
  mappable = axes['WL'].imshow(W0.T, cmap=wcmap, vmin=-15.5,vmax=15.5)
  axes['WR'].imshow(W1.T, cmap=wcmap, vmin=-15.5,vmax=15.5)
  cb = pl.colorbar(mappable, cax=axes['CB'], orientation='horizontal')
  cb.set_ticks(np.linspace(-15,15,7))
  cb.ax.xaxis.set_tick_params(size=0)
  cb.set_label("Inh/Exc weight (4 bit)", labelpad=2., fontsize=10.)
  set_axis_grid(axes['WL'])
  set_axis_grid(axes['WR'])
  #
  # plot synapse drivers
  for drv in driver[0]:
    draw_driver(drv, axes['DL'])
  for drv in driver[1]:
    draw_driver(drv, axes['DR'])
  beautify_driver_axes(axes['DL'], ytickpos='left')
  beautify_driver_axes(axes['DR'], ytickpos='right')
  # plot neurons
  for nrn in neuron[0]:
    draw_neuron(nrn, axes['NL'])
  for nrn in neuron[1]:
    draw_neuron(nrn, axes['NR'])
  beautify_neuron_axes(axes['NL'])
  beautify_neuron_axes(axes['NR'])
  # make labels of weight matrix invisible (must be done afterwards due to sharex/y)
  pl.setp(axes['WL'].get_xticklabels(), visible=False)
  pl.setp(axes['WR'].get_xticklabels(), visible=False)
  pl.setp(axes['WL'].get_yticklabels(), visible=False)
  pl.setp(axes['WR'].get_yticklabels(), visible=False)
  # correct labeling (nrn and drv numbers) in right block
  axes['NR'].set_xticklabels(axes['NR'].get_xticks() + nNeuronPerBlock)
  axes['DR'].set_yticklabels(axes['DR'].get_yticks() + nDrvPerBlock)
  # make legend
  ax = axes['LEGS']
  spines = ('top', 'bottom', 'left', 'right')
  for s in spines:
    ax.spines[s].set_visible(False)
  # show the plot
  pl.interactive(True)
  pl.show()


# # # # # # # # # # # # # # # # # # # # # # # #
# # # Z O O M I N G   C A P A B I L I T Y # # #
# # # # # # # # # # # # # # # # # # # # # # # #

# Add some magic for resizing the synapse drivers upon weight matrix zooming :)
# reference limits
ylimW = {'WL' : axes['WL'].get_ylim(), 'WR' : axes['WR'].get_ylim()}
xlimW = {'WL' : axes['WL'].get_xlim(), 'WR' : axes['WR'].get_xlim()}

# readjust the synapse drivers
def update_driver_axes(ax_w, ax_d, wname): # driver and weight axes, wname is 'WL' or 'WR'
  global ylimW
  ymin, ymax = ax_w.get_ylim()
  if not ((ymin, ymax) == ylimW[wname]):
    if VERBOSE: print " > Resizing %s driver axes and text." % dict(WL='left', WR='right')[wname]
    ylimW[wname] = (ymin, ymax)
    y0, y1 = ax_w.get_position().y0, ax_w.get_position().y1
    h = y1 - y0
    wd = h * 3.7 / (ymax-ymin) / rfig
    if wname == 'WL':
      rect = 0.10-wd,y0,wd,h
    elif wname == 'WR':
      rect = 0.90,y0,wd,h
    ax_d.set_position(rect)
    # resize text
    for ch in ax_d.get_children():
      if isinstance(ch, pl.Text):
        ch.set_fontsize(1.5 * nDrvPerBlock/(ymax-ymin))

# readjust the neurons
def update_neuron_axes(ax_w, ax_n, wname): # neuron and weight axes, wname is 'WL' or 'WR'
  global xlimW
  xmin, xmax = ax_w.get_xlim()
  if not ((xmin, xmax) == xlimW[wname]):
    if VERBOSE: print " > Resizing %s neuron axes and text." % dict(WL='left', WR='right')[wname]
    xlimW[wname] = (xmin, xmax)
    x0, x1 = ax_w.get_position().x0, ax_w.get_position().x1
    w = x1 - x0
    hn = w * 4.1 / abs(xmax-xmin) * rfig
    b = ax_w.get_position().y0
    if wname == 'WL':
      rect = 0.10,b-hn,w,hn
    elif wname == 'WR':
      rect = 0.90-w,b-hn,w,hn
    ax_n.set_position(rect)
    # resize text
    for ch in ax_n.get_children():
      if isinstance(ch, pl.Text):
        ch.set_fontsize(1.5 * nNeuronPerBlock/abs(xmax-xmin))

# upon draw event
PAUSING = False
def on_draw(event):
  global PAUSING
  if PAUSING:
    return
  PAUSING = True
  update_driver_axes(ax_w=axes['WL'], ax_d=axes['DL'], wname='WL')
  update_driver_axes(ax_w=axes['WR'], ax_d=axes['DR'], wname='WR')
  update_neuron_axes(ax_w=axes['WL'], ax_n=axes['NL'], wname='WL')
  update_neuron_axes(ax_w=axes['WR'], ax_n=axes['NR'], wname='WR')
  if VERBOSE: print " > Enter pl.pause()"
  pl.pause(0.75)
  if VERBOSE: print " > Leave pl.pause()"
  PAUSING = False
  pl.draw()

# connect event handler
connect_id_draw = fig.canvas.mpl_connect('draw_event', on_draw)

def on_close(event):
  #fig.canvas.mpl_disconnect(connect_id_draw)
  if VERBOSE: print ' > Window closed.'

fig.canvas.mpl_connect('close_event', on_close)

# save figure
if PLOT and (pngfile is not None):
  print " > Save figure to '%s' at %d dpi." % (pngfile, DPI)
  fig.savefig(pngfile, dpi=DPI)
  if VERBOSE: print " > Done."
else:
  print " > Figure will NOT be saved."
  if VERBOSE: print " > Done."

# If not in interactive mode, we have to block the window
# to prevent python interpreter from exit
# Here, we only check if ipython is running.
try:
  if __IPYTHON__:
    if VERBOSE: print ' > ipython detected. Staying interactive.'
except:
  if VERBOSE: print ' > No ipython detected. Blocking window.'
  pl.interactive(False)
  #pl.show(block=True)
  pl.show()
