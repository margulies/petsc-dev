#!/usr/bin/env python

configure_options = [
  '--with-shared=1',
  '--with-mpi-shared=0',
  '--with-mpi-dir=/soft/apps/packages/mpich-gm-1.2.6..13b-gcc-3.2.3-1',

  '-COPTFLAGS=-O2 -march=pentium4 -mcpu=pentium4',
  '-FOPTFLAGS=-O2 -march=pentium4 -mcpu=pentium4',
  '--with-debuging=0',

  '--with-lapack-lib=/soft/apps/packages/lapack-3.0/lib/lapack_LINUX.a',
  '--with-blas-lib=/usr/lib/libblas.a'

  ]

if __name__ == '__main__':
  import configure
  configure.petsc_configure(configure_options)

# Extra options used for testing locally
test_options = []
