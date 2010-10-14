/*
      Regular array object, for easy parallelism of simple grid 
   problems on regular distributed arrays.
*/
#if !defined(__PETSCDA_H)
#define __PETSCDA_H
#include "petscvec.h"
#include "petscao.h"
PETSC_EXTERN_CXX_BEGIN

EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMInitializePackage(const char[]);

/*S
     DM - Abstract PETSc object that manages an abstract grid object
          
   Level: intermediate

  Concepts: grids, grid refinement

   Notes: The DA object and the Composite object are examples of DMs

          Though the DA objects require the petscsnes.h include files the DM library is
    NOT dependent on the SNES or KSP library. In fact, the KSP and SNES libraries depend on
    DM. (This is not great design, but not trivial to fix).

.seealso:  DMCompositeCreate(), DACreate()
S*/
typedef struct _p_DM* DM;

/*E
    DAStencilType - Determines if the stencil extends only along the coordinate directions, or also
      to the northeast, northwest etc

   Level: beginner

.seealso: DACreate1d(), DACreate2d(), DACreate3d(), DA, DACreate()
E*/
typedef enum { DA_STENCIL_STAR,DA_STENCIL_BOX } DAStencilType;

/*MC
     DA_STENCIL_STAR - "Star"-type stencil. In logical grid coordinates, only (i,j,k), (i+s,j,k), (i,j+s,k),
                       (i,j,k+s) are in the stencil  NOT, for example, (i+s,j+s,k)

     Level: beginner

.seealso: DA_STENCIL_BOX, DAStencilType
M*/

/*MC
     DA_STENCIL_BOX - "Box"-type stencil. In logical grid coordinates, any of (i,j,k), (i+s,j+r,k+t) may 
                      be in the stencil.

     Level: beginner

.seealso: DA_STENCIL_STAR, DAStencilType
M*/

/*E
    DAPeriodicType - Is the domain periodic in one or more directions

   Level: beginner

   DA_XYZGHOSTED means that ghost points are put around all the physical boundaries
   in the local representation of the Vec (i.e. DACreate/GetLocalVector().

.seealso: DACreate1d(), DACreate2d(), DACreate3d(), DA, DACreate()
E*/
typedef enum { DA_NONPERIODIC,DA_XPERIODIC,DA_YPERIODIC,DA_XYPERIODIC,
               DA_XYZPERIODIC,DA_XZPERIODIC,DA_YZPERIODIC,DA_ZPERIODIC,DA_XYZGHOSTED} DAPeriodicType;
extern const char *DAPeriodicTypes[];

/*E
    DAInterpolationType - Defines the type of interpolation that will be returned by 
       DAGetInterpolation.

   Level: beginner

.seealso: DACreate1d(), DACreate2d(), DACreate3d(), DA, DAGetInterpolation(), DASetInterpolationType(), DACreate()
E*/
typedef enum { DA_Q0, DA_Q1 } DAInterpolationType;

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetInterpolationType(DM,DAInterpolationType);

/*E
    DAElementType - Defines the type of elements that will be returned by 
       DMGetElements()

   Level: beginner

.seealso: DACreate1d(), DACreate2d(), DACreate3d(), DMGetInterpolation(), DASetInterpolationType(), 
          DASetElementType(), DMGetElements(), DMRestoreElements(), DACreate()
E*/
typedef enum { DA_ELEMENT_P1, DA_ELEMENT_Q1 } DAElementType;

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetElementType(DM,DAElementType);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetElementType(DM,DAElementType*);

#define DAXPeriodic(pt) ((pt)==DA_XPERIODIC||(pt)==DA_XYPERIODIC||(pt)==DA_XZPERIODIC||(pt)==DA_XYZPERIODIC)
#define DAYPeriodic(pt) ((pt)==DA_YPERIODIC||(pt)==DA_XYPERIODIC||(pt)==DA_YZPERIODIC||(pt)==DA_XYZPERIODIC)
#define DAZPeriodic(pt) ((pt)==DA_ZPERIODIC||(pt)==DA_XZPERIODIC||(pt)==DA_YZPERIODIC||(pt)==DA_XYZPERIODIC)

typedef enum { DA_X,DA_Y,DA_Z } DADirection;

extern PetscClassId PETSCDM_DLLEXPORT DM_CLASSID;

#define MATSEQUSFFT        "sequsfft"

EXTERN PetscErrorCode PETSCDM_DLLEXPORT DACreate(MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetDim(DM,PetscInt);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetSizes(DM,PetscInt,PetscInt,PetscInt);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACreate1d(MPI_Comm,DAPeriodicType,PetscInt,PetscInt,PetscInt,const PetscInt[],DM *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACreate2d(MPI_Comm,DAPeriodicType,DAStencilType,PetscInt,PetscInt,PetscInt,PetscInt,PetscInt,PetscInt,const PetscInt[],const PetscInt[],DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACreate3d(MPI_Comm,DAPeriodicType,DAStencilType,PetscInt,PetscInt,PetscInt,PetscInt,PetscInt,PetscInt,PetscInt,PetscInt,const PetscInt[],const PetscInt[],const PetscInt[],DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMSetOptionsPrefix(DM,const char []);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMSetVecType(DM,const VecType);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGlobalToNaturalBegin(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGlobalToNaturalEnd(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DANaturalToGlobalBegin(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DANaturalToGlobalEnd(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DALocalToLocalBegin(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DALocalToLocalEnd(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACreateGlobalVector(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACreateLocalVector(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DARefine(DM,MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACoarsen(DM,MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DARefineHierarchy(DM,PetscInt,DM[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACoarsenHierarchy(DM,PetscInt,DM[]);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DACreateNaturalVector(DM,Vec *);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DALoad(PetscViewer,PetscInt,PetscInt,PetscInt,DM *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetCorners(DM,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetGhostCorners(DM,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetInfo(DM,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,PetscInt*,DAPeriodicType*,DAStencilType*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetProcessorSubset(DM,DADirection,PetscInt,MPI_Comm*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetProcessorSubsets(DM,DADirection,MPI_Comm*);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGlobalToNaturalAllCreate(DM,VecScatter*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DANaturalAllToGlobalCreate(DM,VecScatter*);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetGlobalIndices(DM,PetscInt*,PetscInt**);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetISLocalToGlobalMapping(DM,ISLocalToGlobalMapping*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetISLocalToGlobalMappingBlck(DM,ISLocalToGlobalMapping*);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetScatter(DM,VecScatter*,VecScatter*,VecScatter*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetNeighbors(DM,const PetscMPIInt**);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetAO(DM,AO*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DASetCoordinates(DM,Vec); 
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetCoordinates(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetGhostedCoordinates(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetCoordinateDA(DM,DM *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DASetUniformCoordinates(DM,PetscReal,PetscReal,PetscReal,PetscReal,PetscReal,PetscReal);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetBoundingBox(DM,PetscReal[],PetscReal[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetLocalBoundingBox(DM,PetscReal[],PetscReal[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DASetFieldName(DM,PetscInt,const char[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAGetFieldName(DM,PetscInt,const char**);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetPeriodicity(DM, DAPeriodicType);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetDof(DM, int);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetStencilWidth(DM, PetscInt);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetOwnershipRanges(DM,const PetscInt[],const PetscInt[],const PetscInt[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DAGetOwnershipRanges(DM,const PetscInt**,const PetscInt**,const PetscInt**);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetNumProcs(DM, PetscInt, PetscInt, PetscInt);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DASetStencilType(DM, DAStencilType);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAVecGetArray(DM,Vec,void *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAVecRestoreArray(DM,Vec,void *);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAVecGetArrayDOF(DM,Vec,void *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DAVecRestoreArrayDOF(DM,Vec,void *);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    DASplitComm2d(MPI_Comm,PetscInt,PetscInt,PetscInt,MPI_Comm*);

/* Dynamic creation and loading functions */
#define DMType char*
extern PetscFList DMList;
extern PetscBool  DMRegisterAllCalled;
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMSetType(DM, const DMType);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMGetType(DM, const DMType *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMRegister(const char[],const char[],const char[],PetscErrorCode (*)(DM));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMRegisterAll(const char []);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT DMRegisterDestroy(void);

/*MC
  DMRegisterDynamic - Adds a new DM component implementation

  Synopsis:
  PetscErrorCode DMRegisterDynamic(const char *name,const char *path,const char *func_name, PetscErrorCode (*create_func)(DM))

  Not Collective

  Input Parameters:
+ name        - The name of a new user-defined creation routine
. path        - The path (either absolute or relative) of the library containing this routine
. func_name   - The name of routine to create method context
- create_func - The creation routine itself

  Notes:
  DMRegisterDynamic() may be called multiple times to add several user-defined DMs

  If dynamic libraries are used, then the fourth input argument (routine_create) is ignored.

  Sample usage:
.vb
    DMRegisterDynamic("my_da","/home/username/my_lib/lib/libO/solaris/libmy.a", "MyDMCreate", MyDMCreate);
.ve

  Then, your DM type can be chosen with the procedural interface via
.vb
    DMCreate(MPI_Comm, DM *);
    DMSetType(DM,"my_da_name");
.ve
   or at runtime via the option
.vb
    -da_type my_da_name
.ve

  Notes: $PETSC_ARCH occuring in pathname will be replaced with appropriate values.
         If your function is not being put into a shared library then use DMRegister() instead
        
  Level: advanced

.keywords: DM, register
.seealso: DMRegisterAll(), DMRegisterDestroy(), DMRegister()
M*/
#if defined(PETSC_USE_DYNAMIC_LIBRARIES)
#define DMRegisterDynamic(a,b,c,d) DMRegister(a,b,c,0)
#else
#define DMRegisterDynamic(a,b,c,d) DMRegister(a,b,c,d)
#endif

EXTERN PetscErrorCode PETSCDM_DLLEXPORT    MatRegisterDAAD(void);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT    MatCreateDAAD(DM,Mat*);
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT   MatCreateSeqUSFFT(Vec, DM,Mat*);

/*S
     DALocalInfo - C struct that contains information about a structured grid and a processors logical
              location in it.

   Level: beginner

  Concepts: distributed array

  Developer note: Then entries in this struct are int instead of PetscInt so that the elements may
                  be extracted in Fortran as if from an integer array

.seealso:  DACreate1d(), DACreate2d(), DACreate3d(), DMDestroy(), DM, DAGetLocalInfo(), DAGetInfo()
S*/
typedef struct {
  PetscInt       dim,dof,sw;
  PetscInt       mx,my,mz;    /* global number of grid points in each direction */
  PetscInt       xs,ys,zs;    /* starting pointd of this processor, excluding ghosts */
  PetscInt       xm,ym,zm;    /* number of grid points on this processor, excluding ghosts */
  PetscInt       gxs,gys,gzs;    /* starting point of this processor including ghosts */
  PetscInt       gxm,gym,gzm;    /* number of grid points on this processor including ghosts */
  DAPeriodicType pt;
  DAStencilType  st;
  DM             da;
} DALocalInfo;

/*MC
      DAForEachPointBegin2d - Starts a loop over the local part of a two dimensional DA

   Synopsis:
   void  DAForEachPointBegin2d(DALocalInfo *info,PetscInt i,PetscInt j);
   
   Not Collective

   Level: intermediate

.seealso: DAForEachPointEnd2d(), DAVecGetArray()
M*/
#define DAForEachPointBegin2d(info,i,j) {\
  PetscInt _xints = info->xs,_xinte = info->xs+info->xm,_yints = info->ys,_yinte = info->ys+info->ym;\
  for (j=_yints; j<_yinte; j++) {\
    for (i=_xints; i<_xinte; i++) {\

/*MC
      DAForEachPointEnd2d - Ends a loop over the local part of a two dimensional DA

   Synopsis:
   void  DAForEachPointEnd2d;
   
   Not Collective

   Level: intermediate

.seealso: DAForEachPointBegin2d(), DAVecGetArray()
M*/
#define DAForEachPointEnd2d }}}

/*MC
      DACoor2d - Structure for holding 2d (x and y) coordinates.

    Level: intermediate

    Sample Usage:
      DACoor2d **coors;
      Vec      vcoors;
      DM       cda;     

      DAGetCoordinates(da,&vcoors); 
      DAGetCoordinateDA(da,&cda);
      DAVecGetArray(cda,vcoors,&coors);
      DAGetCorners(cda,&mstart,&nstart,0,&m,&n,0)
      for (i=mstart; i<mstart+m; i++) {
        for (j=nstart; j<nstart+n; j++) {
          x = coors[j][i].x;
          y = coors[j][i].y;
          ......
        }
      }
      DAVecRestoreArray(dac,vcoors,&coors);

.seealso: DACoor3d, DAForEachPointBegin(), DAGetCoordinateDA(), DAGetCoordinates(), DAGetGhostCoordinates()
M*/
typedef struct {PetscScalar x,y;} DACoor2d;

/*MC
      DACoor3d - Structure for holding 3d (x, y and z) coordinates.

    Level: intermediate

    Sample Usage:
      DACoor3d ***coors;
      Vec      vcoors;
      DM       cda;     

      DAGetCoordinates(da,&vcoors); 
      DAGetCoordinateDA(da,&cda);
      DAVecGetArray(cda,vcoors,&coors);
      DAGetCorners(cda,&mstart,&nstart,&pstart,&m,&n,&p)
      for (i=mstart; i<mstart+m; i++) {
        for (j=nstart; j<nstart+n; j++) {
          for (k=pstart; k<pstart+p; k++) {
            x = coors[k][j][i].x;
            y = coors[k][j][i].y;
            z = coors[k][j][i].z;
          ......
        }
      }
      DAVecRestoreArray(dac,vcoors,&coors);

.seealso: DACoor2d, DAForEachPointBegin(), DAGetCoordinateDA(), DAGetCoordinates(), DAGetGhostCoordinates()
M*/
typedef struct {PetscScalar x,y,z;} DACoor3d;
    
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetLocalInfo(DM,DALocalInfo*);
typedef PetscErrorCode (*DALocalFunction1)(DALocalInfo*,void*,void*,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunctionLocal(DM, DALocalFunction1, Vec, Vec, void *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunctionLocalGhost(DM, DALocalFunction1, Vec, Vec, void *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormJacobianLocal(DM, DALocalFunction1, Vec, Mat, void *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunction1(DM,Vec,Vec,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunction(DM,PetscErrorCode (*)(void),Vec,Vec,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunctioni1(DM,PetscInt,Vec,PetscScalar*,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunctionib1(DM,PetscInt,Vec,PetscScalar*,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAComputeJacobian1WithAdic(DM,Vec,Mat,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAComputeJacobian1WithAdifor(DM,Vec,Mat,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAMultiplyByJacobian1WithAdic(DM,Vec,Vec,Vec,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAMultiplyByJacobian1WithAdifor(DM,Vec,Vec,Vec,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAMultiplyByJacobian1WithAD(DM,Vec,Vec,Vec,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAComputeJacobian1(DM,Vec,Mat,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetLocalFunction(DM,DALocalFunction1*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalFunction(DM,DALocalFunction1);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalFunctioni(DM,PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,PetscScalar*,void*));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalFunctionib(DM,PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,PetscScalar*,void*));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetLocalJacobian(DM,DALocalFunction1*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalJacobian(DM,DALocalFunction1);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalAdicFunction_Private(DM,DALocalFunction1);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  MatSetDA(Mat,DM);

/*MC
       DASetLocalAdicFunction - Caches in a DM a local function computed by ADIC/ADIFOR

   Synopsis:
   PetscErrorCode DASetLocalAdicFunction(DM da,DALocalFunction1 ad_lf)
   
   Logically Collective on DM

   Input Parameter:
+  da - initial distributed array
-  ad_lf - the local function as computed by ADIC/ADIFOR

   Level: intermediate

.keywords:  distributed array, refine

.seealso: DACreate1d(), DACreate2d(), DACreate3d(), DMDestroy(), DAGetLocalFunction(), DASetLocalFunction(),
          DASetLocalJacobian()
M*/
#if defined(PETSC_HAVE_ADIC)
#  define DASetLocalAdicFunction(a,d) DASetLocalAdicFunction_Private(a,(DALocalFunction1)d)
#else
#  define DASetLocalAdicFunction(a,d) DASetLocalAdicFunction_Private(a,0)
#endif

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalAdicMFFunction_Private(DM,DALocalFunction1);
#if defined(PETSC_HAVE_ADIC)
#  define DASetLocalAdicMFFunction(a,d) DASetLocalAdicMFFunction_Private(a,(DALocalFunction1)d)
#else
#  define DASetLocalAdicMFFunction(a,d) DASetLocalAdicMFFunction_Private(a,0)
#endif
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalAdicFunctioni_Private(DM,PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*));
#if defined(PETSC_HAVE_ADIC)
#  define DASetLocalAdicFunctioni(a,d) DASetLocalAdicFunctioni_Private(a,(PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*))d)
#else
#  define DASetLocalAdicFunctioni(a,d) DASetLocalAdicFunctioni_Private(a,0)
#endif
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalAdicMFFunctioni_Private(DM,PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*));
#if defined(PETSC_HAVE_ADIC)
#  define DASetLocalAdicMFFunctioni(a,d) DASetLocalAdicMFFunctioni_Private(a,(PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*))d)
#else
#  define DASetLocalAdicMFFunctioni(a,d) DASetLocalAdicMFFunctioni_Private(a,0)
#endif

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalAdicFunctionib_Private(DM,PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*));
#if defined(PETSC_HAVE_ADIC)
#  define DASetLocalAdicFunctionib(a,d) DASetLocalAdicFunctionib_Private(a,(PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*))d)
#else
#  define DASetLocalAdicFunctionib(a,d) DASetLocalAdicFunctionib_Private(a,0)
#endif
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetLocalAdicMFFunctionib_Private(DM,PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*));
#if defined(PETSC_HAVE_ADIC)
#  define DASetLocalAdicMFFunctionib(a,d) DASetLocalAdicMFFunctionib_Private(a,(PetscErrorCode (*)(DALocalInfo*,MatStencil*,void*,void*,void*))d)
#else
#  define DASetLocalAdicMFFunctionib(a,d) DASetLocalAdicMFFunctionib_Private(a,0)
#endif

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAFormFunctioniTest1(DM,void*);

#include "petscmat.h"


EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMView(DM,PetscViewer);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMDestroy(DM);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCreateGlobalVector(DM,Vec*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCreateLocalVector(DM,Vec*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetLocalVector(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMRestoreLocalVector(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetGlobalVector(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMRestoreGlobalVector(DM,Vec *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetColoring(DM,ISColoringType,const MatType,ISColoring*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetMatrix(DM, const MatType,Mat*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetInterpolation(DM,DM,Mat*,Vec*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetInjection(DM,DM,VecScatter*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMRefine(DM,MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCoarsen(DM,MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMRefineHierarchy(DM,PetscInt,DM[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCoarsenHierarchy(DM,PetscInt,DM[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSetFromOptions(DM);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSetUp(DM);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetInterpolationScale(DM,DM,Mat,Vec*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetAggregates(DM,DM,Mat*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGlobalToLocalBegin(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGlobalToLocalEnd(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMLocalToGlobalBegin(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMLocalToGlobalEnd(DM,Vec,InsertMode,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetElements(DM,PetscInt *,const PetscInt*[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMRestoreElements(DM,PetscInt *,const PetscInt*[]);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSetContext(DM,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMGetContext(DM,void**);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSetInitialGuess(DM,PetscErrorCode (*)(DM,Vec));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSetFunction(DM,PetscErrorCode (*)(DM,Vec,Vec));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSetJacobian(DM,PetscErrorCode (*)(DM,Vec,Mat,Mat,MatStructure *));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMHasInitialGuess(DM,PetscBool *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMHasFunction(DM,PetscBool *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMHasJacobian(DM,PetscBool *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMComputeInitialGuess(DM,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMComputeFunction(DM,Vec,Vec);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMComputeJacobian(DM,Vec,Mat,Mat,MatStructure *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMComputeJacobianDefault(DM,Vec,Mat,Mat,MatStructure *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMFinalizePackage(void);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetColoring(DM,ISColoringType,const MatType,ISColoring *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetMatrix(DM, const MatType,Mat *);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetGetMatrix(DM,PetscErrorCode (*)(DM, const MatType,Mat *));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetInterpolation(DM,DM,Mat*,Vec*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetAggregates(DM,DM,Mat*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetInjection(DM,DM,VecScatter*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetBlockFills(DM,PetscInt*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetMatPreallocateOnly(DM,PetscBool );
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DASetRefinementFactor(DM,PetscInt,PetscInt,PetscInt);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetRefinementFactor(DM,PetscInt*,PetscInt*,PetscInt*);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetAdicArray(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DARestoreAdicArray(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetAdicMFArray(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetAdicMFArray4(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetAdicMFArray9(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetAdicMFArrayb(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DARestoreAdicMFArray(DM,PetscBool ,void*,void*,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DAGetArray(DM,PetscBool ,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DARestoreArray(DM,PetscBool ,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  ad_DAGetArray(DM,PetscBool ,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  ad_DARestoreArray(DM,PetscBool ,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  admf_DAGetArray(DM,PetscBool ,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  admf_DARestoreArray(DM,PetscBool ,void*);

#include "petscpf.h"
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DACreatePF(DM,PF*);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeCreate(MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeAddArray(DM,PetscMPIInt,PetscInt);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeAddDM(DM,DM);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeSetCoupling(DM,PetscErrorCode (*)(DM,Mat,PetscInt*,PetscInt*,PetscInt,PetscInt,PetscInt,PetscInt));
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeSetContext(DM,void*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetContext(DM,void**);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeAddVecScatter(DM,VecScatter);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeScatter(DM,Vec,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGather(DM,Vec,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetAccess(DM,Vec,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetNumberDM(DM,PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeRestoreAccess(DM,Vec,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetLocalVectors(DM,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetEntries(DM,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeRestoreLocalVectors(DM,...);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetLocalISs(DM,IS*[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMCompositeGetGlobalISs(DM,IS*[]);

EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSlicedCreate(MPI_Comm,DM*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSlicedGetGlobalIndices(DM,PetscInt*[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSlicedSetPreallocation(DM,PetscInt,const PetscInt[],PetscInt,const PetscInt[]);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSlicedSetBlockFills(DM,const PetscInt*,const PetscInt*);
EXTERN PetscErrorCode PETSCDM_DLLEXPORT  DMSlicedSetGhosts(DM,PetscInt,PetscInt,PetscInt,const PetscInt[]);


typedef struct NLF_DAAD* NLF;

#include <petscbag.h>

EXTERN PetscErrorCode PETSCSYS_DLLEXPORT PetscViewerBinaryMatlabOpen(MPI_Comm, const char [], PetscViewer*);
EXTERN PetscErrorCode PETSCSYS_DLLEXPORT PetscViewerBinaryMatlabDestroy(PetscViewer);
EXTERN PetscErrorCode PETSCSYS_DLLEXPORT PetscViewerBinaryMatlabOutputBag(PetscViewer, const char [], PetscBag);
EXTERN PetscErrorCode PETSCSYS_DLLEXPORT PetscViewerBinaryMatlabOutputVec(PetscViewer, const char [], Vec);
EXTERN PetscErrorCode PETSCSYS_DLLEXPORT PetscViewerBinaryMatlabOutputVecDA(PetscViewer, const char [], Vec, DM);


PetscErrorCode PETSCDM_DLLEXPORT DMADDACreate(MPI_Comm,PetscInt,PetscInt*,PetscInt*,PetscInt,PetscBool *,DM*);
PetscErrorCode PETSCDM_DLLEXPORT DMADDASetRefinement(DM, PetscInt *,PetscInt);
PetscErrorCode PETSCDM_DLLEXPORT DMADDAGetCorners(DM, PetscInt **, PetscInt **);
PetscErrorCode PETSCDM_DLLEXPORT DMADDAGetGhostCorners(DM, PetscInt **, PetscInt **);
PetscErrorCode PETSCDM_DLLEXPORT DMADDAGetMatrixNS(DM, DM, const MatType , Mat *);

/* functions to set values in vectors and matrices */
struct _ADDAIdx_s {
  PetscInt     *x;               /* the coordinates, user has to make sure it is the correct size! */
  PetscInt     d;                /* indexes the dof */
};
typedef struct _ADDAIdx_s ADDAIdx;

PetscErrorCode PETSCDM_DLLEXPORT DMADDAMatSetValues(Mat, DM, PetscInt, const ADDAIdx[], DM, PetscInt, const ADDAIdx[], const PetscScalar[], InsertMode);
PetscBool  ADDAHCiterStartup(const PetscInt, const PetscInt *const, const PetscInt *const, PetscInt *const);
PetscBool  ADDAHCiter(const PetscInt, const PetscInt *const, const PetscInt *const, PetscInt *const);

PETSC_EXTERN_CXX_END
#endif
