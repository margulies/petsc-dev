#!/usr/bin/env python
#!/bin/env python
# $Id: update-docs.py,v 1.7 2001/08/30 17:51:36 bsmith Exp $ 
#
# update-docs.py LOC
# update-docs.py LOC clean
#

import os
import glob
import posixpath
import string
from sys import *
import shutil

def modifyfile(filename):
    print 'processing file : ' + filename

    import re
    try:
        fd = open(filename,'r')
    except:
        print 'Error! Cannot open file:',filename
        exit()
    buf    = fd.read()
    fd.close()

    header = string.split(string.split(buf, '<!--##end-->')[0],'<!--##begin-->')[1]
    body = string.split(string.split(buf, '<!--##end-->')[1],'<!--##begin-->')[1]

    outbuf = '<html>\n<body BGCOLOR="FFFFFF">\n' + header + '\n' + body + '</body>\n</html>\n'

    #fix http://www.mcs.anl.gov/petsc/petsc-2/snapshots/petsc-current/docs/
    w = re.compile(r'http://www.mcs.anl.gov/petsc/petsc-2/snapshots/petsc-current/docs/')
    outbuf = w.sub('',outbuf)

    #fix  http://www.mcs.anl.gov/petsc/petsc-2/snapshots/petsc-current/include/ (for petscversion.h)
    w = re.compile(r'http://www.mcs.anl.gov/petsc/petsc-2/snapshots/petsc-current/include/')
    outbuf = w.sub('',outbuf)

    # Now overwrite the original file 
    outfilename = filename
    try:
        fd = open( outfilename,'w')
    except:
        print 'Error writing to file',outfilename
        exit()

    fd.write(outbuf)
    fd.close()
    return

# given docs/installation/unix.htm, check if the dir docs/installation exists.
# if not, create it.
def chkdir(dirname):
    if not os.path.isdir(dirname):
        print 'Creating dir', dirname
        os.mkdir(dirname)

def rmfile(filename):
    if  os.path.isfile(filename):
        os.remove(filename)

def main():
    arg_len = len(argv)

    if arg_len < 2:
        print 'Error Insufficient arguments.'
        print 'Usage:', argv[0], 'LOC'
    LOC = argv[1]

    cleanfiles = 0
    if arg_len == 3:
        if argv[2] == 'clean' :
          cleanfiles = 1
    
    baseurl = 'http://www-unix.mcs.anl.gov/petsc/petsc-2/documentation'
    baseurl = LOC + '/docs/website/documentation/'
    htmlfiles = [
        'bugreporting.html',
        'codemanagement.html',
        'copyright.html',
        'faq.html',
        'index.html',
        'linearsolvertable.html',
        'troubleshooting.html',
        'changes/2015.html',
        'changes/2016.html',
        'changes/2017.html',
        'changes/2022.html',
        'changes/2024.html',
        'changes/2028.html',
        'changes/2029.html',
        'changes/21.html',
        'changes/211.html',
        'changes/212.html',
        'changes/213.html',
        'changes/215.html',
        'changes/216.html',        
        'changes/2918-21.html',
        'changes/index.html',
        'installation.html']

    # if clean option is providedm then delete the files and exit
    if cleanfiles == 1 :
        for basename in htmlfiles:
            urlname  = baseurl + basename
            filename = LOC + '/docs/' + basename
            rmfile(filename)
        exit()

    for basename in htmlfiles:
        urlname  = baseurl + basename
        filename = LOC + '/docs/' + basename
        dirpath = os.path.dirname(filename)
        chkdir(dirpath)
        #wgetcmd = 'wget -nv ' + urlname + ' -O ' + filename
        #os.system(wgetcmd)
        shutil.copyfile(urlname,filename)

        modifyfile(filename)
        
# The classes in this file can also
# be used in other python-programs by using 'import'
if __name__ ==  '__main__': 
      main()
 
