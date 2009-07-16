#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os
import PETSc.package

class Configure(PETSc.package.Package):
  def __init__(self, framework):
    PETSc.package.Package.__init__(self, framework)
    self.download     = ['http://ftp.mcs.anl.gov/pub/petsc/externalpackages/ml-6.2.tar.gz']
    self.functions = ['ML_Set_PrintLevel']
    self.includes  = ['ml_include.h']
    self.liblist   = [['libml.a']]
    self.license   = 'http://trilinos.sandia.gov/'
    self.fc        = 1
    return

  def setupDependencies(self, framework):
    PETSc.package.Package.setupDependencies(self, framework)
    self.mpi        = framework.require('config.packages.MPI',self)
    self.blasLapack = framework.require('config.packages.BlasLapack',self)
    self.deps       = [self.mpi,self.blasLapack]  
    return

  def generateLibList(self,dir):
    '''Normally the one in package.py is used, but ML requires the extra C++ library'''
    libs = ['libml']
    alllibs = []
    for l in libs:
      alllibs.append(l+'.a')
    # Now specify -L ml-lib-path only to the first library
    alllibs[0] = os.path.join(dir,alllibs[0])
    import config.setCompilers
    if self.languages.clanguage == 'C':
      alllibs.extend(self.compilers.cxxlibs)
    return [alllibs]
        
  def Install(self):

    args = ['--prefix='+self.installDir]
    args.append('--disable-ml-epetra')
    args.append('--disable-ml-aztecoo')
    args.append('--disable-ml-examples')
    args.append('--disable-tests')
    
    self.framework.pushLanguage('C')
    args.append('CC="'+self.framework.getCompiler()+'"')
    args.append('--with-cflags="'+self.framework.getCompilerFlags()+' -DMPICH_SKIP_MPICXX"')
    self.framework.popLanguage()

    if hasattr(self.compilers, 'FC'):
      self.framework.pushLanguage('FC')
      args.append('F77="'+self.framework.getCompiler()+'"')
      args.append('--with-fflags="'+self.framework.getCompilerFlags()+'"')
      self.framework.popLanguage()
    else:
      args.append('F77=""')

    if hasattr(self.compilers, 'CXX'):
      self.framework.pushLanguage('Cxx')
      args.append('CXX="'+self.framework.getCompiler()+'"')
      args.append('--with-cxxflags="'+self.framework.getCompilerFlags()+' -DMPICH_SKIP_MPICXX"')
      self.framework.popLanguage()
    else:
      raise RuntimeError('Error: ML requires C++ compiler. None specified')
      
    if self.mpi.directory:
      args.append('--with-mpi="'+self.mpi.directory+'"') 
    else:
      raise RuntimeError("Installing ML requires explicit root directory of MPI\nRun config/configure.py again with the additional argument --with-mpi-dir=rootdir")

    libs = []
    for l in self.mpi.lib:
      ll = os.path.basename(l)
      libs.append('-l'+ll[3:-2])
    libs = ' '.join(libs) # '-lmpich -lpmpich'
    args.append('--with-mpi-libs=" '+libs+'"')
    args.append('--with-blas="'+self.libraries.toString(self.blasLapack.dlib)+'"')
    args = ' '.join(args)
    fd = file(os.path.join(self.packageDir,'ml'), 'w')
    fd.write(args)
    fd.write('\n')
    fd.close()

    if self.installNeeded('ml'):
      try:
        self.logPrintBox('Configuring ml; this may take several minutes')
        output  = config.base.Configure.executeShellCommand('cd '+self.packageDir+'; ./configure '+args, timeout=900, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running configure on ML: '+str(e))
      # Build ML
      try:
        self.logPrintBox('Compiling ml; this may take several minutes')
        output  = config.base.Configure.executeShellCommand('cd '+self.packageDir+'; ML_INSTALL_DIR='+self.installDir+'; export ML_INSTALL_DIR; make clean; make; make install', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running make on ML: '+str(e))

      #need to run ranlib on the libraries using the full path
      try:
        output  = config.base.Configure.executeShellCommand(self.setCompilers.RANLIB+' '+os.path.join(self.installDir,'lib')+'/lib*.a', timeout=2500, log = self.framework.log)[0]
      except RuntimeError, e:
        raise RuntimeError('Error running ranlib on ML libraries: '+str(e))

      self.postInstall(output,'ml')
    return self.installDir
  
  def configureLibrary(self):
    '''Calls the regular package configureLibrary and then does an additional test needed by ML'''
    '''Normally you do not need to provide this method'''
    PETSc.package.Package.configureLibrary(self)
    # ML requires LAPACK routine dgels() ?
    if not self.blasLapack.checkForRoutine('dgels'):
      raise RuntimeError('ML requires the LAPACK routine dgels(), the current Lapack libraries '+str(self.blasLapack.lib)+' does not have it')
    self.framework.log.write('Found dgels() in Lapack library as needed by ML\n')
    return

if __name__ == '__main__':
  import config.framework
  import sys
  framework = config.framework.Framework(sys.argv[1:])
  framework.setupLogging(framework.clArgs)
  framework.children.append(Configure(framework))
  framework.configure()
  framework.dumpSubstitutions()
