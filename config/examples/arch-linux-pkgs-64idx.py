#!/usr/bin/env python

configure_options = [
  '--with-64-bit-indices=1',
  '--download-metis=1',
  '--download-parmetis=1',
  '--download-pastix=1',
  '--download-ptscotch=1',
  '--download-elemental=1',
  '--download-hypre=1',
  '--download-superlu_dist=1',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)
