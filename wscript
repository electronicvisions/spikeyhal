#!/usr/bin/env python
import os
from waflib import Options, Node, Utils
APPNAME='SpikeyHAL'


def depends(ctx):
    ctx('logger')
    ctx('logger', 'pylogging')
    ctx('vmodule')

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.load('python')
    opt.load('boost')
    opt.load('gtest')

    sopts = opt.add_option_group('SpikeyHAL')

    # compile options
    sopts.add_withoption('python', default=True, help='Create python wrapper')
    sopts.add_withoption('testmodes', default=False, help='Compile and link testmodes')
    sopts.add_withoption('test', default=True, help='Automatic (software) tests')

    # reduce noise
    sopts.add_option('--nowarnings', action='store_true', default=False, help='Disable compiler warnings aka hardy-style')


def configure(conf):
    print "==== %s ====" % APPNAME
    conf.load('compiler_c')
    conf.load('compiler_cxx')
    conf.load('python')
    conf.load('boost')

    # compile flags for LIBSPIKEYHAL
    conf.env.CXXFLAGS_LIBSPIKEYHAL  = ('-O0 -g -fPIC -std=gnu++0x').split()
    conf.env.INCLUDES_LIBSPIKEYHAL = [
        '.',
        conf.path.find_dir('.').get_bld().abspath(),
    ]

    #basic sources necessary to build testenvironment for spikey chip, requires only ANSI C++ libs
    conf.env.BASICSRCS = '''
        common.cpp idata.cpp sc_sctrl.cpp sc_pbmem.cpp spikenet.cpp \
        ctrlif.cpp synapse_control.cpp pram_control.cpp spikey.cpp spikeyconfig.cpp hardwareConstants.cpp
     '''.split()

    #extended functionality to create spikey control framework (spikey class, spiketrain etc.) and API for HANNEE based software
    conf.env.MORESRCS = '''
        spikeycalibratable.cpp spikeyvoutcalib.cpp
    '''.split()

    conf.env.WRAPPERSRCS = '''
        bindings/python/pyhal_c_interface_s1v2.cpp \
        bindings/python/wrappers/pyspiketrain.cpp \
        bindings/python/wrappers/pyspikeyconfig.cpp \
        bindings/python/wrappers/pysncomm.cpp \
        bindings/python/wrappers/pysc_mem.cpp \
        bindings/python/wrappers/pyspikey.cpp \
        bindings/python/wrappers/pyhwtest.cpp \
        hardwareConstants.cpp
    '''.split()

    conf.env.TESTMODES= '''
        testmodes/oldmodes.cpp \
        testmodes/tmag_eventhard.cpp \
        testmodes/tmag_loopback.cpp \
        testmodes/tmag_spikeyclass.cpp \
        testmodes/tmag_sctest.cpp
    '''.split()
        #testmodes/tmag_allandi.cpp \
        #testmodes/tmag_alldigi.cpp \



    # Python Wrapper
    conf.env.WITH_PYTHON = conf.options.with_python
    if conf.env.WITH_PYTHON:
        conf.load('boost')
        conf.check_python_version(minver=(2,5))
        conf.check_python_headers()
        conf.check_boost(lib='python', uselib_store='BOOSTPY')
        # compile flags
        conf.env.CXXFLAGS_PYHAL = ['-DSPIKEYHALPATH=../..', ]
    # Testmodes
    conf.env.WITH_TESTMODES = conf.options.with_testmodes
    conf.define('CONFIG_PKG_FILE', "config_pkg-nathan_spikeytest.h") #TP: obsolete?
    conf.env.CXXFLAGS_LIBSPIKEYHAL += ['-DCONFIG_H_AVAILABLE'] # for CONFIG_PKG_FILE
    # no warnings (aka HARDY STYLE)
    if not Options.options.nowarnings:
        conf.env.CXXFLAGS_LIBSPIKEYHAL += ('-Wall -Wextra -pedantic -Winline').split() # old-gcc-whines: -Wstrict-overflow=5
        conf.env.CXXFLAGS_LIBSPIKEYHAL += ('-Wno-long-long -Wno-sign-compare -Wno-parentheses').split()
    conf.env.WITH_TEST = conf.options.with_test
    if conf.env.WITH_TEST:
        conf.load('gtest')
    if not conf.get_define('HAVE_GTEST'):
        # diable tests...
        conf.env.WITH_TEST = False
    conf.write_config_header('config.h')



    # check libraries
    conf.check_cxx(header_name='Qt/qconfig.h',
                   includes='/usr/include/qt4',
                   lib=['QtCore', 'QtXml'],
                   uselib_store='QT4',
                   mandatory=1)
    conf.check_cxx(lib='m', uselib_store='M', mandatory=1)
    conf.check_cxx(lib='pthread', uselib_store='PTHREAD', mandatory=1)
    conf.check_cxx(lib=['gsl','gslcblas'], uselib_store='GSLCBLAS', mandatory=1)
    conf.check_cxx(lib='rt', uselib_store='RT', mandatory=1)



def build(bld):
    #if bld.env.enable_flansch: #set in vmodule
        #bld.env.CXXFLAGS_LIBSPIKEYHAL += ['-DWITH_FLANSCH']

    bld(
        features='c cstlib',
        source='getch.c',
        target='getch',
        install_path=None,
    )

    installPathTests = '${PREFIX}/bin/tests'

    # build libspikeyhal
    bld(
        features     = 'cxx c cstlib',
        source       = bld.env.BASICSRCS + bld.env.MORESRCS,
        target       = 'spikeyhal',
        includes     = '.',
        use          = 'logger_obj M RT PTHREAD GSLCBLAS QT4 BOOST_THREAD LIBSPIKEYHAL' \
                       + ' vmodule_objects_inc' \
                       + (bld.env.LIBUSBDUMMY and ' libusb_dummy' or ' LIBUSB') \
                       + (bld.env.WITH_TEST and ' GTEST' or '') \
                       + (bld.env.FLYSPI and ' vmodule_objects' or ''),
                       #+ (bld.env.enable_flansch and ' flansch_software' or ''),

        export_includes = bld.env.INCLUDES_LIBSPIKEYHAL,
        install_path = None,
        )

    for filename in bld.path.ant_glob('tools/*.cpp'):
        split = Node.split_path(filename.abspath())
        bld.program (
            target = os.path.splitext(split[len(split)-1])[0],
            features = 'cxx cxxprogram',
            source = [filename],
            use = 'vmodule_objects logger_obj spikeyhal',
            install_path = 'bin'
        )

    if bld.env.WITH_PYTHON:
        bld(
            features     = 'cxx cshlib pyext',
            source       = bld.env.WRAPPERSRCS,
            includes     = ['.', 'bindings/python', 'bindings/python/wrappers'],
            use          ='vmodule_objects logger_obj spikeyhal',
            uselib       = 'PY BOOSTPY PYHAL',
            target       = 'pyhal_c_interface_s1v2',
            install_path = '${PREFIX}/lib',
        )

    #build testmodes
    if bld.env.WITH_TESTMODES:
        bld(
            features     = 'cxx cxxprogram',
            source       = ['createtb.cpp'] + bld.env.TESTMODES,
            includes     = '.',
            target       = 'testmodes-main',
            use          = 'vmodule_objects logger_obj spikeyhal getch',
            install_path = installPathTests,
        )

    bld.install_as(
            '${PREFIX}/bin/scvisual',
            'tools/scvisual.py',
            chmod=Utils.O755,
    )

    bld.install_files(
            '${PREFIX}/lib',
            'tools/scparse.py',
            chmod=Utils.O755,
    )

    bld.install_files(
            '${PREFIX}/lib',
            'tools/spikey_gold_label_medium.png',
            chmod=Utils.O644,
    )

    #build gtests
    if bld.env.WITH_TEST:
        from waflib.extras.gtest import summary
        test_main_filter = ['*Test*']
        bld(
            target       = 'test-main',
            features     = 'gtest cxx cxxprogram',
            test_main    = 'tests/gtest_main.cpp',
            source       = bld.path.ant_glob('tests/**/*.cpp', excl=['tests/gtest_main.cpp']),
            use          = 'vmodule_objects logger_obj spikeyhal GTEST',
            defines      = 'GTESTPATH="%s"' % os.path.join(bld.path.abspath(), 'tests'),
            test_environ = { "GTEST_FILTER" : "-" + ":".join(test_main_filter) },
            install_path = installPathTests,
        )
        bld.install_files(installPathTests, ['tests/networks/spiketrain_decorrnetwork.in', 'tests/networks/spikeyconfig_decorrnetwork.out'])
        #bld.add_post_fun(summary)
