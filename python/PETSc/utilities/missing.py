#!/usr/bin/env python
from __future__ import generators
import user
import config.base

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.compilers    = self.framework.require('config.compilers', self)
    self.functions    = self.framework.require('config.functions', self)
    self.libraries    = self.framework.require('config.libraries', self)
    return

  def __str__(self):
    return ''
    
  def setupHelp(self, help):
    import nargs
    return

  def checkPrototype(self, includes = '', body = '', cleanup = 1, codeBegin = None, codeEnd = None):
    (output, error, status) = self.outputCompile(includes, body, cleanup, codeBegin, codeEnd)
    output += error
    if output.find('implicit') >= 0 or output.find('Implicit') >= 0:
      return 0
    return 1

#-------------------------------------------------------
  def configureMissingDefines(self):
    '''Checks for limits'''
    if not self.checkCompile('#ifdef PETSC_HAVE_LIMITS_H\n  #include <limits.h>\n#endif\n', 'int i=INT_MAX;\n\nif (i);\n'):
      self.addDefine('INT_MIN', '(-INT_MAX - 1)')
      self.addDefine('INT_MAX', 2147483647)
    if not self.checkCompile('#ifdef PETSC_HAVE_FLOAT_H\n  #include <float.h>\n#endif\n', 'double d=DBL_MAX;\n\nif (d);\n'):
      self.addDefine('DBL_MIN', 2.2250738585072014e-308)
      self.addDefine('DBL_MAX', 1.7976931348623157e+308)
    return

  def configureMissingFunctions(self):
    '''Checks for SOCKETS'''
    if not self.functions.haveFunction('socket'):
      # solaris requires these two libraries for socket()
      if self.libraries.haveLib('socket') and self.libraries.haveLib('nsl'):
        # check if it can find the function
        if self.functions.check('socket',['-lsocket','-lnsl']):
          self.addDefine('HAVE_SOCKET', 1)
          self.framework.argDB['LIBS'] += ' -lsocket -lnsl'
        
      # Windows requires Ws2_32.lib for socket(), uses stdcall, and declspec prototype decoration
      if self.libraries.add('Ws2_32.lib','socket',prototype='#include <Winsock2.h>',call='socket(0,0,0);'):
        self.addDefine('HAVE_WINSOCK2_H',1)
        self.addDefine('HAVE_SOCKET', 1)
        if self.checkLink('#include <Winsock2.h>','closesocket(0)'):
          self.addDefine('HAVE_CLOSESOCKET',1)
        if self.checkLink('#include <Winsock2.h>','WSAGetLastError()'):
          self.addDefine('HAVE_WSAGETLASTERROR',1)
    return

  def configureMissingSignals(self):
    '''Check for missing signals, and define MISSING_<signal name> if necessary'''
    for signal in ['ABRT', 'ALRM', 'BUS',  'CHLD', 'CONT', 'FPE',  'HUP',  'ILL', 'INT',  'KILL', 'PIPE', 'QUIT', 'SEGV',
                   'STOP', 'SYS',  'TERM', 'TRAP', 'TSTP', 'URG',  'USR1', 'USR2']:
      if not self.checkCompile('#include <signal.h>\n', 'int i=SIG'+signal+';\n\nif (i);\n'):
        self.addDefine('MISSING_SIG'+signal, 1)
    return


  def configureMissingErrnos(self):
    '''Check for missing errno values, and define MISSING_<errno value> if necessary'''
    for errnoval in ['EINTR']:
      if not self.checkCompile('#include <errno.h>','int i='+errnoval+';\n\nif (i);\n'):
        self.addDefine('MISSING_ERRNO_'+errnoval, 1)
    return
  

  def configureMissingPrototypes(self):
    if not self.checkPrototype('#include <unistd.h>\n','char test[10]; int err = getdomainname(test,10);'):
      self.addPrototype('int getdomainname(char *, int);', 'C')
    if 'CXX' in self.framework.argDB:
      self.pushLanguage('C++')
      if not self.checkLink('#include <unistd.h>\n','char test[10]; int err = getdomainname(test,10);'):
        self.addPrototype('int getdomainname(char *, int);', 'extern C')
      self.popLanguage()  
    return

  def configure(self):
    self.executeTest(self.configureMissingDefines)
    self.executeTest(self.configureMissingFunctions)
    self.executeTest(self.configureMissingSignals)
    self.executeTest(self.configureMissingPrototypes)
    return
