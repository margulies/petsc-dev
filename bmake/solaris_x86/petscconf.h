#ifdef PETSC_RCS_HEADER
"$Id: petscconf.h,v 1.3 2000/11/28 17:26:31 bsmith Exp $"
"Defines the configuration for this machine"
#endif

#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H

#define PARCH_solaris 
#define PETSC_ARCH_NAME "solaris"

#define PETSC_HAVE_POPEN
#define PETSC_HAVE_LIMITS_H
#define PETSC_HAVE_STROPTS_H 
#define PETSC_HAVE_SEARCH_H 
#define PETSC_HAVE_PWD_H 
#define PETSC_HAVE_STRING_H 
#define PETSC_HAVE_MALLOC_H
#define PETSC_HAVE_STDLIB_H
#define PETSC_HAVE_UNISTD_H 
#define PETSC_HAVE_DRAND48 
#define PETSC_HAVE_GETCWD
#define PETSC_HAVE_SLEEP
#define PETSC_HAVE_SYS_PARAM_H
#define PETSC_HAVE_SYS_STAT_H

#define PETSC_HAVE_SYS_TIME_H
#define PETSC_HAVE_SYS_SYSTEMINFO_H
#define PETSC_HAVE_SYSINFO_3ARG
#define PETSC_HAVE_SUNMATH_H
#define PETSC_HAVE_RESTRICT
#define PETSC_HAVE_SUNMATHPRO

#define PETSC_BITS_PER_BYTE 8

#define PETSC_HAVE_FORTRAN_UNDERSCORE

#define PETSC_HAVE_READLINK
#define PETSC_HAVE_MEMMOVE

#define PETSC_HAVE_DOUBLE_ALIGN_MALLOC

#define PETSC_HAVE_SYS_PROCFS_H
#define PETSC_USE_PROCFS_FOR_SIZE
#define PETSC_HAVE_MEMALIGN
#define PETSC_USE_DBX_DEBUGGER

#define PETSC_USE_DYNAMIC_LIBRARIES 1
#define PETSC_HAVE_RTLD_GLOBAL 1
#define PETSC_HAVE_SYS_TIMES_H
#define PETSC_HAVE_UCBPS

#define PETSC_HAVE_SOLARIS_STYLE_FPTRAP
#define PETSC_HAVE_CXX_NAMESPACE
#endif
