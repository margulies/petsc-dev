#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: vecreg.c,v 1.5 2000/01/10 03:18:14 knepley Exp $";
#endif

#include "src/vec/vecimpl.h"    /*I "petscvec.h"  I*/

PetscFList VecList                       = PETSC_NULL;
PetscTruth VecRegisterAllCalled          = PETSC_FALSE;

#undef __FUNCT__  
#define __FUNCT__ "VecSetType"
/*@C
  VecSetType - Builds a vector, for a particular vector implementation.

  Collective on Vec

  Input Parameters:
+ vec    - The vector object
- method - The name of the vector type

  Options Database Key:
. -vec_type <type> - Sets the vector type; use -help for a list 
                     of available types

  Notes:
  See "petsc/include/vec.h" for available vector types (for instance, VECSEQ, VECMPI, or VECSHARED).

  Use VecDuplicate() or VecDuplicateVecs() to form additional vectors of the same type as an existing vector.

  Level: intermediate

.keywords: vector, set, type
.seealso: VecGetType(), VecCreate()
@*/
int VecSetType(Vec vec, const VecType method)
{
  int      (*r)(Vec);
  PetscTruth match;
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_COOKIE,1);
  ierr = PetscTypeCompare((PetscObject) vec, method, &match);                                             CHKERRQ(ierr);
  if (match == PETSC_TRUE) PetscFunctionReturn(0);

  /* Get the function pointers for the vector requested */
  if (VecRegisterAllCalled == PETSC_FALSE) {
    ierr = VecRegisterAll(PETSC_NULL);                                                                    CHKERRQ(ierr);
  }
  ierr = PetscFListFind(vec->comm, VecList, method,(void (**)(void)) &r);                                 CHKERRQ(ierr);
  if (!r) SETERRQ1(PETSC_ERR_ARG_WRONG, "Unknown vector type: %s", method);

  if (vec->ops->destroy != PETSC_NULL) {
    ierr = (*vec->ops->destroy)(vec);                                                                     CHKERRQ(ierr);
  }
  ierr = (*r)(vec);                                                                                       CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject) vec, method);                                            CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "VecGetType"
/*@C
  VecGetType - Gets the vector type name (as a string) from the Vec.

  Not Collective

  Input Parameter:
. vec  - The vector

  Output Parameter:
. type - The vector type name

  Level: intermediate

.keywords: vector, get, type, name
.seealso: VecSetType(), VecCreate()
@*/
int VecGetType(Vec vec, VecType *type)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_COOKIE,1);
  PetscValidCharPointer(type,2);
  if (VecRegisterAllCalled == PETSC_FALSE) {
    ierr = VecRegisterAll(PETSC_NULL);                                                                    CHKERRQ(ierr);
  }
  *type = vec->type_name;
  PetscFunctionReturn(0);
}


/*--------------------------------------------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "VecRegister"
/*@C
  VecRegister - See VecRegisterDynamic()

  Level: advanced
@*/
int VecRegister(const char sname[], const char path[], const char name[], int (*function)(Vec))
{
  char fullname[PETSC_MAX_PATH_LEN];
  int  ierr;

  PetscFunctionBegin;
  ierr = PetscStrcpy(fullname, path);                                                                     CHKERRQ(ierr);
  ierr = PetscStrcat(fullname, ":");                                                                      CHKERRQ(ierr);
  ierr = PetscStrcat(fullname, name);                                                                     CHKERRQ(ierr);
  ierr = PetscFListAdd(&VecList, sname, fullname, (void (*)(void)) function);                             CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/*--------------------------------------------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "VecRegisterDestroy"
/*@C
   VecRegisterDestroy - Frees the list of Vec methods that were registered by VecRegister()/VecRegisterDynamic().

   Not Collective

   Level: advanced

.keywords: Vec, register, destroy
.seealso: VecRegister(), VecRegisterAll(), VecRegisterDynamic()
@*/
int VecRegisterDestroy(void)
{
  int ierr;

  PetscFunctionBegin;
  if (VecList != PETSC_NULL) {
    ierr = PetscFListDestroy(&VecList);                                                                   CHKERRQ(ierr);
    VecList = PETSC_NULL;
  }
  VecRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

