#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os
import PETSc.package

class Configure(PETSc.package.Package):
  def __init__(self, framework):
    PETSc.package.Package.__init__(self, framework)
    self.download   = ['http://ftp.mcs.anl.gov/pub/petsc/externalpackages/SuperLU_DIST_2.3-hg.tar.gz']
    self.functions  = ['set_default_options_dist']
    self.includes   = ['superlu_ddefs.h']
    self.liblist    = [['libsuperlu_dist_2.3.a']]
    #
    #  SuperLU_dist supports 64 bit integers but uses ParMetis which does not, it has
    #  a hack that uses the 32 bit parmetis
    #  SuperLU_dist's support for 64 bit integers is nonsense! (Fortran code -qintsize=8 compile options)
    self.requires32bitint = 1;
    self.complex    = 1
    return

  def setupDependencies(self, framework):
    PETSc.package.Package.setupDependencies(self, framework)
    self.mpi        = framework.require('config.packages.MPI',self)
    self.blasLapack = framework.require('config.packages.BlasLapack',self)    
    self.parmetis = framework.require('PETSc.packages.ParMetis',self)
    self.deps       = [self.mpi,self.blasLapack,self.parmetis]
    return

  def Install(self):

    g = open(os.path.join(self.packageDir,'make.inc'),'w')
    g.write('DSuperLUroot = '+self.packageDir+'\n')
    g.write('DSUPERLULIB  = $(DSuperLUroot)/libsuperlu_dist_2.3.a\n')
    g.write('BLASDEF      = -DUSE_VENDOR_BLAS\n')
    g.write('BLASLIB      = '+self.libraries.toString(self.blasLapack.dlib)+'\n')
    g.write('IMPI         = '+self.headers.toString(self.mpi.include)+'\n')
    g.write('MPILIB       = '+self.libraries.toString(self.mpi.lib)+'\n')
    g.write('PMETISLIB    = '+self.libraries.toString(self.parmetis.lib)+'\n')
    g.write('LIBS         = $(DSUPERLULIB) $(BLASLIB) $(PMETISLIB) $(MPILIB)\n')
    g.write('ARCH         = '+self.setCompilers.AR+'\n')
    g.write('ARCHFLAGS    = '+self.setCompilers.AR_FLAGS+'\n')
    g.write('RANLIB       = '+self.setCompilers.RANLIB+'\n')
    self.setCompilers.pushLanguage('C')
    g.write('CC           = '+self.setCompilers.getCompiler()+' $(IMPI)\n') #build fails without $(IMPI)
    g.write('CFLAGS       = '+self.setCompilers.getCompilerFlags()+'\n')
    g.write('LOADER       = '+self.setCompilers.getLinker()+'\n') 
    g.write('LOADOPTS     = \n')
    self.setCompilers.popLanguage()
    if self.blasLapack.mangling == 'underscore':
      g.write('CDEFS   = -DAdd_')
    elif self.blasLapack.mangling == 'caps':
      g.write('CDEFS   = -DUpCase')
    else:
      g.write('CDEFS   = -DNoChange')
    if self.framework.argDB['with-64-bit-indices']:
      g.write(' -D_LONGINT')
    g.write('\n')
    if hasattr(self.compilers, 'FC'):
      self.setCompilers.pushLanguage('FC')
      g.write('FORTRAN      = '+self.setCompilers.getCompiler()+'\n')
      g.write('FFLAGS       = '+self.setCompilers.getCompilerFlags().replace('-Mfree','')+'\n')
      # set fortran name mangling
      # this mangling information is for both BLAS and the Fortran compiler so cannot use the BlasLapack mangling flag      
      self.setCompilers.popLanguage()
    g.write('NOOPTS       = '+self.blasLapack.getSharedFlag(self.setCompilers.getCompilerFlags())+' '+self.blasLapack.getPrecisionFlag(self.setCompilers.getCompilerFlags())+' '+self.blasLapack.getWindowsNonOptFlags(self.setCompilers.getCompilerFlags())+'\n')
    g.close()

    if self.installNeeded('make.inc'):
      try:
        self.logPrintBox('Compiling superlu_dist; this may take several minutes')
        output  = config.base.Configure.executeShellCommand('cd '+self.packageDir+';SUPERLU_DIST_INSTALL_DIR='+self.installDir+'/lib;export SUPERLU_DIST_INSTALL_DIR; make clean; make lib LAAUX=""; mv -f *.a '+os.path.join(self.installDir,'lib')+'; cp -f SRC/*.h '+os.path.join(self.installDir,'include')+'/.', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running make on SUPERLU_DIST: '+str(e))
      self.checkInstall(output,'make.inc')
    return self.installDir

  def configureLibrary(self):
    '''Calls the regular package configureLibrary and then does an additional test needed by SuperLU_DIST'''
    '''Normally you do not need to provide this method'''
    PETSc.package.Package.configureLibrary(self)
    if not self.blasLapack.checkForRoutine('slamch'): 
      raise RuntimeError('SuperLU_DIST requires the BLAS routine slamch()')
    self.framework.log.write('Found slamch() in BLAS library as needed by SuperLU_DIST\n')

    if not self.blasLapack.checkForRoutine('dlamch'): 
      raise RuntimeError('SuperLU_DIST requires the BLAS routine dlamch()')
    self.framework.log.write('Found dlamch() in BLAS library as needed by SuperLU_DIST\n')
    if not self.blasLapack.checkForRoutine('xerbla'): 
      raise RuntimeError('SuperLU_DIST requires the BLAS routine xerbla()')
    self.framework.log.write('Found xerbla() in BLAS library as needed by SuperLU_DIST\n')
    return
  
if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setupLogging(framework.clArgs)
  framework.children.append(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
