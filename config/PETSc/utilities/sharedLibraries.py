#!/usr/bin/env python
from __future__ import generators
import user
import config.base

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.useShared    = 0
    self.useDynamic   = 0
    return

  def __str1__(self):
    if not hasattr(self, 'useShared') or not hasattr(self, 'useDynamic'):
      return ''
    txt = ''
    if self.useShared:
      txt += '  shared libraries: enabled\n'
    else:
      txt += '  shared libraries: disabled\n'
    if self.useDynamic:
      txt += '  dynamic libraries: enabled\n'
    else:
      txt += '  dynamic libraries: disabled\n'
    return txt

  def setupHelp(self, help):
    import nargs
    help.addArgument('PETSc', '-with-shared-libraries', nargs.ArgBool(None, 0, 'Make PETSc libraries shared -- libpetsc.so (Unix/Linux) or libpetsc.dylib (Mac)'))
    help.addArgument('PETSc', '-with-dynamic-loading', nargs.ArgBool(None, 0, 'Make PETSc libraries dynamic -- uses dlopen() to access libraries, rarely needed'))
    return

  def setupDependencies(self, framework):
    config.base.Configure.setupDependencies(self, framework)
    self.arch = framework.require('PETSc.utilities.arch', self)
    self.setCompilers = framework.require('config.setCompilers', self)
    return

  def configureSharedLibraries(self):
    '''Checks whether dynamic libraries should be used, for which you must
      - Specify --with-shared-libraries
      - Have found a working dynamic linker
    Defines PETSC_USE_SHARED_LIBRARIES if they are used'''
    if self.argDB['with-dynamic-loading'] and not self.argDB['with-shared-libraries']:
      raise RuntimeError('If you use --with-dynamic-loading you also need --with-shared-libraries')

    if self.argDB['with-shared-libraries'] and not self.argDB['with-pic'] :
      raise RuntimeError('If you use --with-shared-libraries you cannot turn off pic with --with-pic=0')
    
    self.useShared = self.argDB['with-shared-libraries'] and not self.setCompilers.staticLibraries
    if self.useShared:
      if config.setCompilers.Configure.isSolaris() and config.setCompilers.Configure.isGNU(self.framework.getCompiler()):
        self.addMakeRule('shared_arch','shared_'+self.arch.hostOsBase+'gnu')
      else:
        self.addMakeRule('shared_arch','shared_'+self.arch.hostOsBase)
      self.addMakeMacro('BUILDSHAREDLIB','yes')
    else:
      self.addMakeRule('shared_arch','')
      self.addMakeMacro('BUILDSHAREDLIB','no')
    if self.setCompilers.sharedLibraries:
      self.addDefine('HAVE_SHARED_LIBRARIES', 1)
    if self.useShared:
      self.addDefine('USE_SHARED_LIBRARIES', 1)
    else:
      self.logPrint('Shared libraries - disabled')
    return

  def configureDynamicLibraries(self):
    '''Checks whether dynamic libraries should be used, for which you must
      - Specify --with-dynamic-loading
      - Have found a working dynamic linker (with dlfcn.h and libdl)
    Defines PETSC_USE_DYNAMIC_LIBRARIES if they are used'''
    if self.setCompilers.dynamicLibraries:
      self.addDefine('HAVE_DYNAMIC_LIBRARIES', 1)
    self.useDynamic = self.argDB['with-dynamic-loading'] and self.useShared and self.setCompilers.dynamicLibraries
    if self.useDynamic:
      self.addDefine('USE_DYNAMIC_LIBRARIES', 1)
    else:
      self.logPrint('Dynamic libraries - disabled')
    return

  def configure(self):
    self.executeTest(self.configureSharedLibraries)
    self.executeTest(self.configureDynamicLibraries)
    return
