/* $Id: petsclog.h,v 1.155 2001/09/06 14:51:20 bsmith Exp $ */

/*
    Defines profile/logging in PETSc.
*/

#if !defined(__PetscLog_H)
#define __PetscLog_H
#include "petsc.h"  

/*
  Each PETSc object class has it's own cookie (internal integer in the 
  data structure used for error checking). These are all defined by an offset 
  from the lowest one, PETSC_COOKIE.
*/
#define PETSC_COOKIE 1211211
extern int PETSC_LARGEST_COOKIE;
#define PETSC_EVENT  1311311
extern int PETSC_LARGEST_EVENT;

/* Events for the Petsc standard library */
extern int PETSC_Barrier;

/* Global flop counter */
extern PetscLogDouble _TotalFlops;

/* General logging of information; different from event logging */
EXTERN int        PetscLogInfo(void*,const char[],...) __ATTRIBUTE__ ((format (printf, 2,3)));
EXTERN int        PetscLogInfoDeactivateClass(int);
EXTERN int        PetscLogInfoActivateClass(int);
extern PetscTruth PetscLogPrintInfo;  /* if true, indicates PetscLogInfo() is turned on */

#if defined(PETSC_USE_LOG)  /* --- Logging is turned on --------------------------------*/

/* 
   Flop counting:  We count each arithmetic operation (e.g., addition, multiplication) separately.

   For the complex numbers version, note that
       1 complex addition = 2 flops
       1 complex multiplication = 6 flops,
   where we define 1 flop as that for a double precision scalar.  We roughly approximate
   flop counting for complex numbers by multiplying the total flops by 4; this corresponds
   to the assumption that we're counting mostly additions and multiplications -- and
   roughly the same number of each.  More accurate counting could be done by distinguishing
   among the various arithmetic operations.
 */

#if defined(PETSC_USE_COMPLEX)
#define PetscLogFlops(n) (_TotalFlops += (4*n),0)
#else
#define PetscLogFlops(n) (_TotalFlops += (n),0)
#endif

#if defined (PETSC_HAVE_MPE)
#include "mpe.h"
#define MPEBEGIN    1000 
EXTERN int        PetscLogMPEBegin(void);
EXTERN int        PetscLogMPEDump(const char[]);
extern PetscTruth UseMPE;
extern int        PetscLogEventMPEFlags[];
EXTERN int        PetscLogEventMPEActivate(int);
EXTERN int        PetscLogEventMPEDeactivate(int);
#else
#define PetscLogEventMPEActivate(a) 0
#define PetscLogEventMPEDeactivate(a) 0
#endif

EXTERN int (*_PetscLogPLB)(int,int,PetscObject,PetscObject,PetscObject,PetscObject);
EXTERN int (*_PetscLogPLE)(int,int,PetscObject,PetscObject,PetscObject,PetscObject);
EXTERN int (*_PetscLogPHC)(PetscObject);
EXTERN int (*_PetscLogPHD)(PetscObject);

#define PetscLogObjectParent(p,c)       if (c) {PetscValidHeader((PetscObject)(c)); \
                                                PetscValidHeader((PetscObject)(p));\
                                                ((PetscObject)(c))->parent = (PetscObject)(p);\
				                ((PetscObject)(c))->parentid = ((PetscObject)p)->id;}
#define PetscLogObjectParents(p,n,d)    {int _i; for (_i=0; _i<n; _i++) \
                                         PetscLogObjectParent(p,(d)[_i]);}
#define PetscLogObjectCreate(h)         {if (_PetscLogPHC) (*_PetscLogPHC)((PetscObject)h);}
#define PetscLogObjectDestroy(h)        {if (_PetscLogPHD) (*_PetscLogPHD)((PetscObject)h);}
#define PetscLogObjectMemory(p,m)       {PetscValidHeader((PetscObject)p);\
                                         ((PetscObject)(p))->mem += (m);}
/* Initialization functions */
EXTERN int PetscLogBegin(void);
EXTERN int PetscLogAllBegin(void);
EXTERN int PetscLogTraceBegin(FILE *);
/* General functions */
EXTERN int PetscLogDestroy(void);
EXTERN int PetscLogSet(int (*)(int, int, PetscObject, PetscObject, PetscObject, PetscObject),
                   int (*)(int, int, PetscObject, PetscObject, PetscObject, PetscObject));
EXTERN int PetscLogObjectState(PetscObject, const char[], ...) __ATTRIBUTE__ ((format (printf, 2,3)));
/* Output functions */
EXTERN int PetscLogPrintSummary(MPI_Comm, const char[]);
EXTERN int PetscLogDump(const char[]);
/* Counter functions */
EXTERN int PetscGetFlops(PetscLogDouble *);
/* Stage functions */
EXTERN int PetscLogStageRegister(int *, const char[]);
EXTERN int PetscLogStagePush(int);
EXTERN int PetscLogStagePop(void);
EXTERN int PetscLogStageSetVisible(int, PetscTruth);
EXTERN int PetscLogStageGetVisible(int, PetscTruth *);
EXTERN int PetscLogStageGetId(const char [], int *);
/* Event functions */
EXTERN int PetscLogEventRegister(int *, const char[], const char[], int);
EXTERN int PetscLogEventActivate(int);
EXTERN int PetscLogEventDeactivate(int);
EXTERN int PetscLogEventActivateClass(int);
EXTERN int PetscLogEventDeactivateClass(int);
/* Class functions */
EXTERN int PetscLogClassRegister(int *, const char []);

/* Default log */
typedef struct _StageLog *StageLog;
extern StageLog _stageLog;

/* Global counters */
extern PetscLogDouble irecv_ct,  isend_ct,  recv_ct,  send_ct;
extern PetscLogDouble irecv_len, isend_len, recv_len, send_len;
extern PetscLogDouble allreduce_ct;
extern PetscLogDouble wait_ct, wait_any_ct, wait_all_ct, sum_of_waits_ct;
extern int            PETSC_DUMMY, PETSC_DUMMY_SIZE;

/* We must make these structures available if we are to access the event
   activation flags in the PetscLogEventBegin/End() macros. If we forced a
   function call each time, we could leave these structures in plog.h
*/
/* The structure for logging performance */
typedef struct _PerfInfo {
  char          *name;          /* The name of this section */
  char          *color;         /* The color of this section */
  int            id;            /* The integer identifying this event */
  int            cookie;        /* The class id for this section */
  PetscTruth     active;        /* The flag to activate logging */
  PetscTruth     visible;       /* The flag to print info in summary */
  int            depth;         /* The nesting depth of the event call */
  int            count;         /* The number of times this section was executed */
  PetscLogDouble flops;         /* The flops used in this section */
  PetscLogDouble time;          /* The time taken for this section */
  PetscLogDouble numMessages;   /* The number of messages in this section */
  PetscLogDouble messageLength; /* The total message lengths in this section */
  PetscLogDouble numReductions; /* The number of reductions in this section */
} PerfInfo;

/* The structure for logging events */
typedef struct _EventLog *EventLog;
struct _EventLog {
  int       numEvents;   /* The number of registered events */
  int       maxEvents;   /* The maximum number of events */
  PerfInfo *eventInfo;   /* The performance information for each event */
};

/* The structure for logging class information */
typedef struct _ClassInfo {
  char          *name;          /* The class name */
  int            cookie;        /* The integer identifying this class */
  int            creations;     /* The number of objects of this class created */
  int            destructions;  /* The number of objects of this class destroyed */
  PetscLogDouble mem;           /* The total memory allocated by objects of this class */
  PetscLogDouble descMem;       /* The total memory allocated by descendents of these objects */
} ClassInfo;

typedef struct _ClassLog *ClassLog;
struct _ClassLog {
  int        numClasses; /* The number of classes registered */
  int        maxClasses; /* The maximum number of classes */
  ClassInfo *classInfo;  /* The structure for classs information (cookies are monotonicly increasing) */
};

/* A simple stack (should replace) */
typedef struct _IntStack *IntStack;

/* The structure for logging in stages */
struct _StageLog {
  /* Size information */
  int         numStages;    /* The number of registered stages */
  int         maxStages;    /* The maximum number of stages */
  /* Runtime information */
  IntStack    stack;        /* The stack for active stages */
  int         curStage;     /* The current stage (only used in macros so we don't call StackTop) */
  /* Stage specific information */
  PerfInfo   *stageInfo;    /* The performance information for each stage */
  EventLog   *eventLog;     /* The event log for each stage */
  ClassLog   *classLog;     /* The class information for each stage */
};

EXTERN int EventLogGetEvent(EventLog, int, int *);

#if defined(PETSC_HAVE_MPE)
#define PetscLogEventBarrierBegin(e,o1,o2,o3,o4,cm) \
  0; { int _1_ierr, _1_eventNum; \
    _1_ierr = EventLogGetEvent(_stageLog->eventLog[_stageLog->curStage], (e), &_1_eventNum); \
    if (_PetscLogPLB && _stageLog->eventLog[_stageLog->curStage]->eventInfo[_1_eventNum].active) { \
      _1_ierr = PetscLogEventBegin((e),o1,o2,o3,o4);CHKERRQ(_1_ierr);  \
      if (UseMPE && PetscLogEventMPEFlags[(e)])                      \
        MPE_Log_event(MPEBEGIN+2*(e),0,"");                      \
      _1_ierr = MPI_Barrier(cm);CHKERRQ(_1_ierr);                  \
      _1_ierr = PetscLogEventEnd((e),o1,o2,o3,o4);CHKERRQ(_1_ierr);    \
      if (UseMPE && PetscLogEventMPEFlags[(e)])                      \
        MPE_Log_event(MPEBEGIN+2*((e)+1),0,"");                  \
    }                                                            \
    _1_ierr = PetscLogEventBegin(e+1,o1,o2,o3,o4);CHKERRQ(_1_ierr);    \
    if (UseMPE && PetscLogEventMPEFlags[(e)+1])                      \
      MPE_Log_event(MPEBEGIN+2*((e)+1),0,"");                    \
  }
#define PetscLogEventBegin(e,o1,o2,o3,o4)  \
  0; { int _3_ierr, _3_eventNum; \
    _3_ierr = EventLogGetEvent(_stageLog->eventLog[_stageLog->curStage], (e), &_3_eventNum); \
   if (_PetscLogPLB && _stageLog->eventLog[_stageLog->curStage]->eventInfo[_3_eventNum].active) {\
     (*_PetscLogPLB)((e),0,(PetscObject)(o1),(PetscObject)(o2),(PetscObject)(o3),(PetscObject)(o4));}\
   if (UseMPE && PetscLogEventMPEFlags[(e)])\
     MPE_Log_event(MPEBEGIN+2*(e),0,"");\
  }
#else
#define PetscLogEventBarrierBegin(e,o1,o2,o3,o4,cm) \
  0; { int _2_ierr, _2_eventNum;\
    _2_ierr = EventLogGetEvent(_stageLog->eventLog[_stageLog->curStage], (e), &_2_eventNum); \
    if (_PetscLogPLB && _stageLog->eventLog[_stageLog->curStage]->eventInfo[_2_eventNum].active) {                         \
      _2_ierr = PetscLogEventBegin((e),o1,o2,o3,o4);CHKERRQ(_2_ierr);    \
      _2_ierr = MPI_Barrier(cm);CHKERRQ(_2_ierr);                    \
      _2_ierr = PetscLogEventEnd((e),o1,o2,o3,o4);CHKERRQ(_2_ierr);      \
    }                                                              \
    _2_ierr = PetscLogEventBegin((e)+1,o1,o2,o3,o4);CHKERRQ(_2_ierr);    \
  }
#define PetscLogEventBegin(e,o1,o2,o3,o4)  \
  0; { int _4_ierr, _4_eventNum; \
    _4_ierr = EventLogGetEvent(_stageLog->eventLog[_stageLog->curStage], (e), &_4_eventNum); \
   if (_PetscLogPLB && _stageLog->eventLog[_stageLog->curStage]->eventInfo[_4_eventNum].active) {\
     (*_PetscLogPLB)((e),0,(PetscObject)(o1),(PetscObject)(o2),(PetscObject)(o3),(PetscObject)(o4));}\
  }
#endif

#if defined(PETSC_HAVE_MPE)
#define PetscLogEventBarrierEnd(e,o1,o2,o3,o4,cm) PetscLogEventEnd(e+1,o1,o2,o3,o4)
#define PetscLogEventEnd(e,o1,o2,o3,o4) \
  0; { int _5_ierr, _5_eventNum; \
    _5_ierr = EventLogGetEvent(_stageLog->eventLog[_stageLog->curStage], (e), &_5_eventNum); \
  if (_PetscLogPLE && _stageLog->eventLog[_stageLog->curStage]->eventInfo[_5_eventNum].active) {\
    (*_PetscLogPLE)((e),0,(PetscObject)(o1),(PetscObject)(o2),(PetscObject)(o3),(PetscObject)(o4));}\
  if (UseMPE && PetscLogEventMPEFlags[(e)])\
     MPE_Log_event(MPEBEGIN+2*(e)+1,0,"");\
  }  
#else
#define PetscLogEventBarrierEnd(e,o1,o2,o3,o4,cm) PetscLogEventEnd(e+1,o1,o2,o3,o4)
#define PetscLogEventEnd(e,o1,o2,o3,o4) \
  0; { int _6_ierr, _6_eventNum; \
    _6_ierr = EventLogGetEvent(_stageLog->eventLog[_stageLog->curStage], (e), &_6_eventNum); \
  if (_PetscLogPLE && _stageLog->eventLog[_stageLog->curStage]->eventInfo[_6_eventNum].active) {\
    (*_PetscLogPLE)((e),0,(PetscObject)(o1),(PetscObject)(o2),(PetscObject)(o3),(PetscObject)(o4));}\
  } 
#endif

/*
     This does not work for MPI-Uni because our src/mpiuni/mpi.h file
   uses macros to defined the MPI operations. 

     It does not work correctly from HP-UX because it processes the 
   macros in a way that sometimes it double counts, hence 
   HAVE_BROKEN_RECURSIVE_MACRO

     It does not work with Windows NT because winmpich lacks MPI_Type_size()
*/
#if !defined(PETSC_HAVE_MPI_UNI) && !defined(HAVE_BROKEN_RECURSIVE_MACRO) && !defined (PETSC_HAVE_MPI_MISSING_TYPESIZE)
/*
   Logging of MPI activities
*/

#define TypeSize(buff,count,type)                                                \
(\
  MPI_Type_size(type,&PETSC_DUMMY_SIZE),buff += ((PetscLogDouble) ((count)*PETSC_DUMMY_SIZE)) \
)

#define MPI_Irecv(buf,count, datatype,source,tag,comm,request)        \
(\
  PETSC_DUMMY = MPI_Irecv(buf,count, datatype,source,tag,comm,request),            \
  irecv_ct++,TypeSize(irecv_len,count,datatype),PETSC_DUMMY                            \
)

#define MPI_Isend(buf,count, datatype,dest,tag,comm,request)          \
(\
  PETSC_DUMMY = MPI_Isend(buf,count, datatype,dest,tag,comm,request),              \
  isend_ct++,  TypeSize(isend_len,count,datatype),PETSC_DUMMY                          \
)

#define MPI_Startall_irecv(count,number,requests)                                     \
(\
  PETSC_DUMMY = MPI_Startall(number,requests),                                                    \
  irecv_ct += (PetscLogDouble)(number),irecv_len += ((PetscLogDouble) ((count)*sizeof(PetscScalar))),PETSC_DUMMY \
)

#define MPI_Startall_isend(count,number,requests)                                    \
(\
  PETSC_DUMMY = MPI_Startall(number,requests),                                                   \
  isend_ct += (PetscLogDouble)(number),isend_len += ((PetscLogDouble) ((count)*sizeof(PetscScalar))),PETSC_DUMMY \
)

#define MPI_Start_isend(count, requests)\
(\
  PETSC_DUMMY = MPI_Start(requests),\
  isend_ct++,isend_len += ((PetscLogDouble) ((count)*sizeof(PetscScalar))),PETSC_DUMMY\
)

#define MPI_Recv(buf,count, datatype,source,tag,comm,status)           \
(\
  PETSC_DUMMY = MPI_Recv(buf,count, datatype,source,tag,comm,status),               \
  recv_ct++,TypeSize(recv_len,count,datatype),PETSC_DUMMY                              \
)

#define MPI_Send(buf,count, datatype,dest,tag,comm)                     \
(\
  PETSC_DUMMY = MPI_Send(buf,count, datatype,dest,tag,comm),                         \
  send_ct++, TypeSize(send_len,count,datatype),PETSC_DUMMY                              \
)

#define MPI_Wait(request,status) \
(\
  wait_ct++,sum_of_waits_ct++,  \
  MPI_Wait(request,status)       \
)

#define MPI_Waitany(a,b,c,d)     \
(\
  wait_any_ct++,sum_of_waits_ct++,\
  MPI_Waitany(a,b,c,d)           \
)

#define MPI_Waitall(count,array_of_requests,array_of_statuses) \
(\
  wait_all_ct++,sum_of_waits_ct += (PetscLogDouble) (count),       \
  MPI_Waitall(count,array_of_requests,array_of_statuses)       \
)

#define MPI_Allreduce(sendbuf, recvbuf,count,datatype,op,comm) \
(\
  allreduce_ct++,MPI_Allreduce(sendbuf,recvbuf,count,datatype,op,comm)\
)

#else

#define MPI_Startall_irecv(count,number,requests) \
(\
  MPI_Startall(number,requests)                 \
)

#define MPI_Startall_isend(count,number,requests) \
(\
  MPI_Startall(number,requests)                 \
)

#define MPI_Start_isend(count, requests) \
(\
  MPI_Start(requests)                   \
)

#endif /* !PETSC_HAVE_MPI_UNI && ! HAVE_BROKEN_RECURSIVE_MACRO */

#else  /* ---Logging is turned off --------------------------------------------*/

#define PetscLogFlops(n) 0

/*
     With logging turned off, then MPE has to be turned off
*/
#define MPEBEGIN                  1000 
#define PetscLogMPEBegin()            0
#define PetscLogMPEDump(a)            0
#define PetscLogEventMPEActivate(a)   0
#define PetscLogEventMPEDeactivate(a) 0

#define PetscLogEventActivate(a)   0
#define PetscLogEventDeactivate(a) 0

#define PetscLogEventActivateClass(a)   0
#define PetscLogEventDeactivateClass(a) 0

#define _PetscLogPLB                        0
#define _PetscLogPLE                        0
#define _PetscLogPHC                        0
#define _PetscLogPHD                        0
#define PetscGetFlops(a)                (*(a) = 0.0,0)
#define PetscLogEventBegin(e,o1,o2,o3,o4)   0
#define PetscLogEventEnd(e,o1,o2,o3,o4)     0
#define PetscLogEventBarrierBegin(e,o1,o2,o3,o4,cm) 0
#define PetscLogEventBarrierEnd(e,o1,o2,o3,o4,cm)   0
#define PetscLogObjectParent(p,c)
#define PetscLogObjectParents(p,n,c)
#define PetscLogObjectCreate(h)
#define PetscLogObjectDestroy(h)
#define PetscLogObjectMemory(p,m)
#define PetscLogDestroy()                   0
#define PetscLogStagePush(a)                0
#define PetscLogStagePop()                  0
#define PetscLogStageRegister(a,b)          0
#define PetscLogStagePrint(a,flg)           0
#define PetscLogPrintSummary(comm,file)     0
#define PetscLogBegin()                     0
#define PetscLogTraceBegin(file)            0
#define PetscLogSet(lb,le)                  0
#define PetscLogAllBegin()                  0
#define PetscLogDump(c)                     0
#define PetscLogEventRegister(a,b,c,d)      0
EXTERN int PetscLogObjectState(PetscObject,const char[],...) __ATTRIBUTE__ ((format (printf, 2,3)));

/* If PETSC_USE_LOG is NOT defined, these still need to be! */
#define MPI_Startall_irecv(count,number,requests) MPI_Startall(number,requests)

#define MPI_Startall_isend(count,number,requests) MPI_Startall(number,requests)

#define MPI_Start_isend(count,requests) MPI_Start(requests)

#endif   /* PETSC_USE_LOG */

extern PetscTruth PetscPreLoadingUsed;       /* true if we are or have done preloading */
extern PetscTruth PetscPreLoadingOn;         /* true if we are currently in a preloading calculation */

#define PreLoadBegin(flag,name) {PetscTruth PreLoading = flag; \
                                 int        PreLoadMax,PreLoadIt,_stageNum,_3_ierr;\
                                 _3_ierr = PetscOptionsGetLogical(PETSC_NULL,"-preload",&PreLoading,PETSC_NULL);CHKERRQ(_3_ierr);\
                                 PreLoadMax = (int)(PreLoading);PetscPreLoadingUsed = PreLoading ? PETSC_TRUE : PetscPreLoadingUsed;\
                                 for (PreLoadIt=0; PreLoadIt<=PreLoadMax; PreLoadIt++) {\
                                   PetscPreLoadingOn = PreLoading;\
                                   _3_ierr = PetscBarrier(PETSC_NULL);CHKERRQ(_3_ierr);\
                                   if (PreLoadIt>0) {\
                                     _3_ierr = PetscLogStageGetId(name,&_stageNum);CHKERRQ(_3_ierr);\
                                   } else {\
                                     _3_ierr = PetscLogStageRegister(&_stageNum,name);CHKERRQ(_3_ierr);\
                                     _3_ierr = PetscLogStageSetVisible(_stageNum,(PetscTruth)(!PreLoadMax || PreLoadIt));\
                                   }\
                                   _3_ierr = PetscLogStagePush(_stageNum);CHKERRQ(_3_ierr);

#define PreLoadEnd()               _3_ierr = PetscLogStagePop();CHKERRQ(_3_ierr);PreLoading = PETSC_FALSE;}}

#define PreLoadStage(name)         _3_ierr = PetscLogStagePop();CHKERRQ(_3_ierr);\
                                   if (PreLoadIt>0) {\
                                     _3_ierr = PetscLogStageGetId(name,&_stageNum);CHKERRQ(_3_ierr);\
                                   } else {\
                                     _3_ierr = PetscLogStageRegister(&_stageNum,name);CHKERRQ(_3_ierr);\
                                     _3_ierr = PetscLogStageSetVisible(_stageNum,(PetscTruth)(!PreLoadMax || PreLoadIt));\
                                   }\
                                   _3_ierr = PetscLogStagePush(_stageNum);CHKERRQ(_3_ierr);
#endif
