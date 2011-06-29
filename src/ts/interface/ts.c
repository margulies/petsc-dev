
#include <private/tsimpl.h>        /*I "petscts.h"  I*/

/* Logging support */
PetscClassId  TS_CLASSID;
PetscLogEvent  TS_Step, TS_PseudoComputeTimeStep, TS_FunctionEval, TS_JacobianEval;

#undef __FUNCT__  
#define __FUNCT__ "TSSetTypeFromOptions"
/*
  TSSetTypeFromOptions - Sets the type of ts from user options.

  Collective on TS

  Input Parameter:
. ts - The ts

  Level: intermediate

.keywords: TS, set, options, database, type
.seealso: TSSetFromOptions(), TSSetType()
*/
static PetscErrorCode TSSetTypeFromOptions(TS ts)
{
  PetscBool      opt;
  const char     *defaultType;
  char           typeName[256];
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (((PetscObject)ts)->type_name) {
    defaultType = ((PetscObject)ts)->type_name;
  } else {
    defaultType = TSEULER;
  }

  if (!TSRegisterAllCalled) {ierr = TSRegisterAll(PETSC_NULL);CHKERRQ(ierr);}
  ierr = PetscOptionsList("-ts_type", "TS method"," TSSetType", TSList, defaultType, typeName, 256, &opt);CHKERRQ(ierr);
  if (opt) {
    ierr = TSSetType(ts, typeName);CHKERRQ(ierr);
  } else {
    ierr = TSSetType(ts, defaultType);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetFromOptions"
/*@
   TSSetFromOptions - Sets various TS parameters from user options.

   Collective on TS

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Options Database Keys:
+  -ts_type <type> - TSEULER, TSBEULER, TSSUNDIALS, TSPSEUDO, TSCN, TSRK, TSTHETA, TSGL, TSSSP
.  -ts_max_steps maxsteps - maximum number of time-steps to take
.  -ts_max_time time - maximum time to compute to
.  -ts_dt dt - initial time step
.  -ts_monitor - print information at each timestep
-  -ts_monitor_draw - plot information at each timestep

   Level: beginner

.keywords: TS, timestep, set, options, database

.seealso: TSGetType()
@*/
PetscErrorCode  TSSetFromOptions(TS ts)
{
  PetscReal      dt;
  PetscBool      opt,flg;
  PetscErrorCode ierr;
  PetscViewer    monviewer;
  char           monfilename[PETSC_MAX_PATH_LEN];
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);
  ierr = PetscOptionsBegin(((PetscObject)ts)->comm, ((PetscObject)ts)->prefix, "Time step options", "TS");CHKERRQ(ierr);

    /* Handle generic TS options */
    ierr = PetscOptionsInt("-ts_max_steps","Maximum number of time steps","TSSetDuration",ts->max_steps,&ts->max_steps,PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-ts_max_time","Time to run to","TSSetDuration",ts->max_time,&ts->max_time,PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-ts_init_time","Initial time","TSSetInitialTime", ts->ptime, &ts->ptime, PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-ts_dt","Initial time step","TSSetInitialTimeStep",ts->initial_time_step,&dt,&opt);CHKERRQ(ierr);
    if (opt) {
      ts->initial_time_step = ts->time_step = dt;
    }

    /* Monitor options */
    ierr = PetscOptionsString("-ts_monitor","Monitor timestep size","TSMonitorDefault","stdout",monfilename,PETSC_MAX_PATH_LEN,&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = PetscViewerASCIIOpen(((PetscObject)ts)->comm,monfilename,&monviewer);CHKERRQ(ierr);
      ierr = TSMonitorSet(ts,TSMonitorDefault,monviewer,(PetscErrorCode (*)(void**))PetscViewerDestroy);CHKERRQ(ierr);
    }
    opt  = PETSC_FALSE;
    ierr = PetscOptionsBool("-ts_monitor_draw","Monitor timestep size graphically","TSMonitorLG",opt,&opt,PETSC_NULL);CHKERRQ(ierr);
    if (opt) {
      ierr = TSMonitorSet(ts,TSMonitorLG,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
    }
    opt  = PETSC_FALSE;
    ierr = PetscOptionsBool("-ts_monitor_solution","Monitor solution graphically","TSMonitorSolution",opt,&opt,PETSC_NULL);CHKERRQ(ierr);
    if (opt) {
      ierr = TSMonitorSet(ts,TSMonitorSolution,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
    }

    /* Handle TS type options */
    ierr = TSSetTypeFromOptions(ts);CHKERRQ(ierr);

    /* Handle specific TS options */
    if (ts->ops->setfromoptions) {
      ierr = (*ts->ops->setfromoptions)(ts);CHKERRQ(ierr);
    }

    /* process any options handlers added with PetscObjectAddOptionsHandler() */
    ierr = PetscObjectProcessOptionsHandlers((PetscObject)ts);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);

  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  /* Handle subobject options */
  if (ts->problem_type == TS_LINEAR) {ierr = SNESSetType(snes,SNESKSPONLY);CHKERRQ(ierr);}
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef  __FUNCT__
#define __FUNCT__ "TSViewFromOptions"
/*@
  TSViewFromOptions - This function visualizes the ts based upon user options.

  Collective on TS

  Input Parameter:
. ts - The ts

  Level: intermediate

.keywords: TS, view, options, database
.seealso: TSSetFromOptions(), TSView()
@*/
PetscErrorCode  TSViewFromOptions(TS ts,const char title[])
{
  PetscViewer    viewer;
  PetscDraw      draw;
  PetscBool      opt = PETSC_FALSE;
  char           fileName[PETSC_MAX_PATH_LEN];
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscOptionsGetString(((PetscObject)ts)->prefix, "-ts_view", fileName, PETSC_MAX_PATH_LEN, &opt);CHKERRQ(ierr);
  if (opt && !PetscPreLoadingOn) {
    ierr = PetscViewerASCIIOpen(((PetscObject)ts)->comm,fileName,&viewer);CHKERRQ(ierr);
    ierr = TSView(ts, viewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  }
  opt = PETSC_FALSE;
  ierr = PetscOptionsGetBool(((PetscObject)ts)->prefix, "-ts_view_draw", &opt,PETSC_NULL);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscViewerDrawOpen(((PetscObject)ts)->comm, 0, 0, 0, 0, 300, 300, &viewer);CHKERRQ(ierr);
    ierr = PetscViewerDrawGetDraw(viewer, 0, &draw);CHKERRQ(ierr);
    if (title) {
      ierr = PetscDrawSetTitle(draw, (char *)title);CHKERRQ(ierr);
    } else {
      ierr = PetscObjectName((PetscObject)ts);CHKERRQ(ierr);
      ierr = PetscDrawSetTitle(draw, ((PetscObject)ts)->name);CHKERRQ(ierr);
    }
    ierr = TSView(ts, viewer);CHKERRQ(ierr);
    ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
    ierr = PetscDrawPause(draw);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSComputeRHSJacobian"
/*@
   TSComputeRHSJacobian - Computes the Jacobian matrix that has been
      set with TSSetRHSJacobian().

   Collective on TS and Vec

   Input Parameters:
+  ts - the TS context
.  t - current timestep
-  x - input vector

   Output Parameters:
+  A - Jacobian matrix
.  B - optional preconditioning matrix
-  flag - flag indicating matrix structure

   Notes: 
   Most users should not need to explicitly call this routine, as it 
   is used internally within the nonlinear solvers. 

   See KSPSetOperators() for important information about setting the
   flag parameter.

   TSComputeJacobian() is valid only for TS_NONLINEAR

   Level: developer

.keywords: SNES, compute, Jacobian, matrix

.seealso:  TSSetRHSJacobian(), KSPSetOperators()
@*/
PetscErrorCode  TSComputeRHSJacobian(TS ts,PetscReal t,Vec X,Mat *A,Mat *B,MatStructure *flg)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,3);
  PetscCheckSameComm(ts,1,X,3);
  if (ts->problem_type != TS_NONLINEAR) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"For TS_NONLINEAR only");
  if (ts->ops->rhsjacobian) {
    ierr = PetscLogEventBegin(TS_JacobianEval,ts,X,*A,*B);CHKERRQ(ierr);
    *flg = DIFFERENT_NONZERO_PATTERN;
    PetscStackPush("TS user Jacobian function");
    ierr = (*ts->ops->rhsjacobian)(ts,t,X,A,B,flg,ts->jacP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(TS_JacobianEval,ts,X,*A,*B);CHKERRQ(ierr);
    /* make sure user returned a correct Jacobian and preconditioner */
    PetscValidHeaderSpecific(*A,MAT_CLASSID,4);
    PetscValidHeaderSpecific(*B,MAT_CLASSID,5);
  } else {
    ierr = MatAssemblyBegin(*A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(*A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    if (*A != *B) {
      ierr = MatAssemblyBegin(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
      ierr = MatAssemblyEnd(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    }
    *flg = ts->rhsmatstructure;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSComputeRHSFunction"
/*@
   TSComputeRHSFunction - Evaluates the right-hand-side function. 

   Collective on TS and Vec

   Input Parameters:
+  ts - the TS context
.  t - current time
-  x - state vector

   Output Parameter:
.  y - right hand side

   Note:
   Most users should not need to explicitly call this routine, as it
   is used internally within the nonlinear solvers.

   If the user did not provide a function but merely a matrix,
   this routine applies the matrix.

   Level: developer

.keywords: TS, compute

.seealso: TSSetRHSFunction(), TSComputeIFunction()
@*/
PetscErrorCode TSComputeRHSFunction(TS ts,PetscReal t,Vec x,Vec y)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidHeaderSpecific(x,VEC_CLASSID,3);
  PetscValidHeaderSpecific(y,VEC_CLASSID,4);

  ierr = PetscLogEventBegin(TS_FunctionEval,ts,x,y,0);CHKERRQ(ierr);
  if (ts->ops->rhsfunction) {
    PetscStackPush("TS user right-hand-side function");
    ierr = (*ts->ops->rhsfunction)(ts,t,x,y,ts->funP);CHKERRQ(ierr);
    PetscStackPop;
  } else if (ts->problem_type == TS_LINEAR) {
    Mat A,B;
    ierr = TSGetMatrices(ts,&A,&B,PETSC_NULL, PETSC_NULL,PETSC_NULL,PETSC_NULL, PETSC_NULL);CHKERRQ(ierr);
    if (ts->ops->rhsmatrix) {
      ts->rhsmatstructure = DIFFERENT_NONZERO_PATTERN;
      PetscStackPush("TS user right-hand-side matrix function");
      ierr = (*ts->ops->rhsmatrix)(ts,t,&A,&B,&ts->rhsmatstructure,ts->jacP);CHKERRQ(ierr);
      PetscStackPop;
      /* call TSSetMatrices() in case the user changed the pointers */
      ierr = TSSetMatrices(ts,A,B,PETSC_NULL, PETSC_NULL,PETSC_NULL,PETSC_NULL, PETSC_NULL,ts->jacP);CHKERRQ(ierr);
    }
    ierr = MatMult(A,x,y);CHKERRQ(ierr);
  } else SETERRQ(((PetscObject)ts)->comm,PETSC_ERR_USER,"No RHS provided, must call TSSetRHSFunction() or TSSetMatrices()");

  ierr = PetscLogEventEnd(TS_FunctionEval,ts,x,y,0);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSComputeIFunction"
/*@
   TSComputeIFunction - Evaluates the DAE residual written in implicit form F(t,X,Xdot)=0

   Collective on TS and Vec

   Input Parameters:
+  ts - the TS context
.  t - current time
.  X - state vector
-  Xdot - time derivative of state vector

   Output Parameter:
.  Y - right hand side

   Note:
   Most users should not need to explicitly call this routine, as it
   is used internally within the nonlinear solvers.

   If the user did did not write their equations in implicit form, this
   function recasts them in implicit form.

   Level: developer

.keywords: TS, compute

.seealso: TSSetIFunction(), TSComputeRHSFunction()
@*/
PetscErrorCode TSComputeIFunction(TS ts,PetscReal t,Vec X,Vec Xdot,Vec Y)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,3);
  PetscValidHeaderSpecific(Xdot,VEC_CLASSID,4);
  PetscValidHeaderSpecific(Y,VEC_CLASSID,5);

  ierr = PetscLogEventBegin(TS_FunctionEval,ts,X,Xdot,Y);CHKERRQ(ierr);
  if (ts->ops->ifunction) {
    PetscStackPush("TS user implicit function");
    ierr = (*ts->ops->ifunction)(ts,t,X,Xdot,Y,ts->funP);CHKERRQ(ierr);
    PetscStackPop;
  } else {
    ierr = TSComputeRHSFunction(ts,t,X,Y);CHKERRQ(ierr);
    /* Convert to implicit form: F(X,Xdot) = Alhs * Xdot - Frhs(X) */
    if (ts->ops->lhsmatrix) {
      ts->lhsmatstructure = DIFFERENT_NONZERO_PATTERN;
      PetscStackPush("TS user left-hand-side matrix function");
      ierr = (*ts->ops->lhsmatrix)(ts,t,&ts->Alhs,&ts->Blhs,&ts->lhsmatstructure,ts->jacP);CHKERRQ(ierr);
      PetscStackPop;
      ierr = VecScale(Y,-1.);CHKERRQ(ierr);
      ierr = MatMultAdd(ts->Alhs,Xdot,Y,Y);CHKERRQ(ierr);
    } else {
      ierr = VecAYPX(Y,-1,Xdot);CHKERRQ(ierr);
    }
  }
  ierr = PetscLogEventEnd(TS_FunctionEval,ts,X,Xdot,Y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSComputeIJacobian"
/*@
   TSComputeIJacobian - Evaluates the Jacobian of the DAE

   Collective on TS and Vec

   Input
      Input Parameters:
+  ts - the TS context
.  t - current timestep
.  X - state vector
.  Xdot - time derivative of state vector
-  shift - shift to apply, see note below

   Output Parameters:
+  A - Jacobian matrix
.  B - optional preconditioning matrix
-  flag - flag indicating matrix structure

   Notes:
   If F(t,X,Xdot)=0 is the DAE, the required Jacobian is

   dF/dX + shift*dF/dXdot

   Most users should not need to explicitly call this routine, as it
   is used internally within the nonlinear solvers.

   TSComputeIJacobian() is valid only for TS_NONLINEAR

   Level: developer

.keywords: TS, compute, Jacobian, matrix

.seealso:  TSSetIJacobian()
@*/
PetscErrorCode  TSComputeIJacobian(TS ts,PetscReal t,Vec X,Vec Xdot,PetscReal shift,Mat *A,Mat *B,MatStructure *flg)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,3);
  PetscValidHeaderSpecific(Xdot,VEC_CLASSID,4);
  PetscValidPointer(A,6);
  PetscValidHeaderSpecific(*A,MAT_CLASSID,6);
  PetscValidPointer(B,7);
  PetscValidHeaderSpecific(*B,MAT_CLASSID,7);
  PetscValidPointer(flg,8);

  *flg = SAME_NONZERO_PATTERN;  /* In case it we're solving a linear problem in which case it wouldn't get initialized below. */
  ierr = PetscLogEventBegin(TS_JacobianEval,ts,X,*A,*B);CHKERRQ(ierr);
  if (ts->ops->ijacobian) {
    PetscStackPush("TS user implicit Jacobian");
    ierr = (*ts->ops->ijacobian)(ts,t,X,Xdot,shift,A,B,flg,ts->jacP);CHKERRQ(ierr);
    PetscStackPop;
  } else {
    if (ts->ops->rhsjacobian) {
      PetscStackPush("TS user right-hand-side Jacobian");
      ierr = (*ts->ops->rhsjacobian)(ts,t,X,A,B,flg,ts->jacP);CHKERRQ(ierr);
      PetscStackPop;
    } else {
      ierr = MatAssemblyBegin(*A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
      ierr = MatAssemblyEnd(*A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
      if (*A != *B) {
        ierr = MatAssemblyBegin(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
        ierr = MatAssemblyEnd(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
      }
    }

    /* Convert to implicit form */
    /* inefficient because these operations will normally traverse all matrix elements twice */
    ierr = MatScale(*A,-1);CHKERRQ(ierr);
    if (ts->Alhs) {
      ierr = MatAXPY(*A,shift,ts->Alhs,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
    } else {
      ierr = MatShift(*A,shift);CHKERRQ(ierr);
    }
    if (*A != *B) {
      ierr = MatScale(*B,-1);CHKERRQ(ierr);
      ierr = MatShift(*B,shift);CHKERRQ(ierr);
    }
  }
  ierr = PetscLogEventEnd(TS_JacobianEval,ts,X,*A,*B);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetRHSFunction"
/*@C
    TSSetRHSFunction - Sets the routine for evaluating the function,
    F(t,u), where U_t = F(t,u).

    Logically Collective on TS

    Input Parameters:
+   ts - the TS context obtained from TSCreate()
.   r - vector to put the computed right hand side (or PETSC_NULL to have it created)
.   f - routine for evaluating the right-hand-side function
-   ctx - [optional] user-defined context for private data for the 
          function evaluation routine (may be PETSC_NULL)

    Calling sequence of func:
$     func (TS ts,PetscReal t,Vec u,Vec F,void *ctx);

+   t - current timestep
.   u - input vector
.   F - function vector
-   ctx - [optional] user-defined function context 

    Important: 
    The user MUST call either this routine or TSSetMatrices().

    Level: beginner

.keywords: TS, timestep, set, right-hand-side, function

.seealso: TSSetMatrices()
@*/
PetscErrorCode  TSSetRHSFunction(TS ts,Vec r,PetscErrorCode (*f)(TS,PetscReal,Vec,Vec,void*),void *ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (r) PetscValidHeaderSpecific(r,VEC_CLASSID,2);
  if (ts->problem_type == TS_LINEAR) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Cannot set function for linear problem");
  ts->ops->rhsfunction = f;
  ts->funP             = ctx;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes,r,SNESTSFormFunction,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetMatrices"
/*@C
   TSSetMatrices - Sets the functions to compute the matrices Alhs and Arhs, 
   where Alhs(t) U_t = Arhs(t) U.

   Logically Collective on TS

   Input Parameters:
+  ts   - the TS context obtained from TSCreate()
.  Arhs - matrix
.  Brhs - preconditioning matrix
.  frhs - the matrix evaluation routine for Arhs and Brhs; use PETSC_NULL (PETSC_NULL_FUNCTION in fortran)
          if Arhs is not a function of t.
.  Alhs - matrix or PETSC_NULL if Alhs is an indentity matrix.
.  Blhs - preconditioning matrix or PETSC_NULL if Blhs is an identity matrix
.  flhs - the matrix evaluation routine for Alhs; use PETSC_NULL (PETSC_NULL_FUNCTION in fortran)
          if Alhs is not a function of t.
.  flag - flag indicating information about the matrix structure of Arhs and Alhs. 
          The available options are
            SAME_NONZERO_PATTERN - Alhs has the same nonzero structure as Arhs
            DIFFERENT_NONZERO_PATTERN - Alhs has different nonzero structure as Arhs
-  ctx  - [optional] user-defined context for private data for the 
          matrix evaluation routine (may be PETSC_NULL)

   Calling sequence of func:
$     func(TS ts,PetscReal t,Mat *A,Mat *B,PetscInt *flag,void *ctx);

+  t - current timestep
.  A - matrix A, where U_t = A(t) U
.  B - preconditioner matrix, usually the same as A
.  flag - flag indicating information about the preconditioner matrix
          structure (same as flag in KSPSetOperators())
-  ctx - [optional] user-defined context for matrix evaluation routine

   Notes:  
   The routine func() takes Mat* as the matrix arguments rather than Mat.  
   This allows the matrix evaluation routine to replace Arhs or Alhs with a 
   completely new new matrix structure (not just different matrix elements)
   when appropriate, for instance, if the nonzero structure is changing
   throughout the global iterations.

   Important: 
   The user MUST call either this routine or TSSetRHSFunction().

   Level: beginner

.keywords: TS, timestep, set, matrix

.seealso: TSSetRHSFunction()
@*/
PetscErrorCode  TSSetMatrices(TS ts,Mat Arhs,Mat Brhs,PetscErrorCode (*frhs)(TS,PetscReal,Mat*,Mat*,MatStructure*,void*),Mat Alhs,Mat Blhs,PetscErrorCode (*flhs)(TS,PetscReal,Mat*,Mat*,MatStructure*,void*),MatStructure flag,void *ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (frhs) ts->ops->rhsmatrix = frhs;
  if (flhs) ts->ops->lhsmatrix = flhs;
  if (ctx)  ts->jacP           = ctx;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetType(snes,SNESKSPONLY);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes,Arhs,Brhs,SNESTSFormJacobian,ts);CHKERRQ(ierr);
  if (Alhs) {
    PetscValidHeaderSpecific(Alhs,MAT_CLASSID,4);
    PetscCheckSameComm(ts,1,Alhs,4);
    ierr = PetscObjectReference((PetscObject)Alhs);CHKERRQ(ierr);
    ierr = MatDestroy(&ts->Alhs);CHKERRQ(ierr);
    ts->Alhs = Alhs;
  }
  if (Blhs) {
    PetscValidHeaderSpecific(Blhs,MAT_CLASSID,4);
    PetscCheckSameComm(ts,1,Blhs,4);
    ierr = PetscObjectReference((PetscObject)Blhs);CHKERRQ(ierr);
    ierr = MatDestroy(&ts->Blhs);CHKERRQ(ierr);
    ts->Blhs = Blhs;
  }
  ts->rhsmatstructure = flag;
  ts->lhsmatstructure = flag;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSGetMatrices"
/*@C
   TSGetMatrices - Returns the matrices Arhs and Alhs at the present timestep,
   where Alhs(t) U_t = Arhs(t) U.

   Not Collective, but parallel objects are returned if TS is parallel

   Input Parameter:
.  ts  - The TS context obtained from TSCreate()

   Output Parameters:
+  Arhs - The right-hand side matrix
.  Alhs - The left-hand side matrix
-  ctx - User-defined context for matrix evaluation routine

   Notes: You can pass in PETSC_NULL for any return argument you do not need.

   Level: intermediate

.seealso: TSSetMatrices(), TSGetTimeStep(), TSGetTime(), TSGetTimeStepNumber(), TSGetRHSJacobian()

.keywords: TS, timestep, get, matrix

@*/
PetscErrorCode  TSGetMatrices(TS ts,Mat *Arhs,Mat *Brhs,TSMatrix *frhs,Mat *Alhs,Mat *Blhs,TSMatrix *flhs,void **ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESGetJacobian(snes,Arhs,Brhs,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  if (frhs) *frhs = ts->ops->rhsmatrix;
  if (Alhs) *Alhs = ts->Alhs;
  if (Blhs) *Blhs = ts->Blhs;
  if (flhs) *flhs = ts->ops->lhsmatrix;
  if (ctx)  *ctx = ts->jacP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetRHSJacobian"
/*@C
   TSSetRHSJacobian - Sets the function to compute the Jacobian of F,
   where U_t = F(U,t), as well as the location to store the matrix.
   Use TSSetMatrices() for linear problems.

   Logically Collective on TS

   Input Parameters:
+  ts  - the TS context obtained from TSCreate()
.  A   - Jacobian matrix
.  B   - preconditioner matrix (usually same as A)
.  f   - the Jacobian evaluation routine
-  ctx - [optional] user-defined context for private data for the 
         Jacobian evaluation routine (may be PETSC_NULL)

   Calling sequence of func:
$     func (TS ts,PetscReal t,Vec u,Mat *A,Mat *B,MatStructure *flag,void *ctx);

+  t - current timestep
.  u - input vector
.  A - matrix A, where U_t = A(t)u
.  B - preconditioner matrix, usually the same as A
.  flag - flag indicating information about the preconditioner matrix
          structure (same as flag in KSPSetOperators())
-  ctx - [optional] user-defined context for matrix evaluation routine

   Notes: 
   See KSPSetOperators() for important information about setting the flag
   output parameter in the routine func().  Be sure to read this information!

   The routine func() takes Mat * as the matrix arguments rather than Mat.  
   This allows the matrix evaluation routine to replace A and/or B with a 
   completely new matrix structure (not just different matrix elements)
   when appropriate, for instance, if the nonzero structure is changing
   throughout the global iterations.

   Level: beginner
   
.keywords: TS, timestep, set, right-hand-side, Jacobian

.seealso: TSDefaultComputeJacobianColor(),
          SNESDefaultComputeJacobianColor(), TSSetRHSFunction(), TSSetMatrices()

@*/
PetscErrorCode  TSSetRHSJacobian(TS ts,Mat A,Mat B,TSRHSJacobian f,void *ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (A) PetscValidHeaderSpecific(A,MAT_CLASSID,2);
  if (B) PetscValidHeaderSpecific(B,MAT_CLASSID,3);
  if (A) PetscCheckSameComm(ts,1,A,2);
  if (B) PetscCheckSameComm(ts,1,B,3);
  if (ts->problem_type != TS_NONLINEAR) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Not for linear problems; use TSSetMatrices()");

  if (f)   ts->ops->rhsjacobian = f;
  if (ctx) ts->jacP             = ctx;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes,A,B,SNESTSFormJacobian,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "TSSetIFunction"
/*@C
   TSSetIFunction - Set the function to compute F(t,U,U_t) where F = 0 is the DAE to be solved.

   Logically Collective on TS

   Input Parameters:
+  ts  - the TS context obtained from TSCreate()
.  r   - vector to hold the residual (or PETSC_NULL to have it created internally)
.  f   - the function evaluation routine
-  ctx - user-defined context for private data for the function evaluation routine (may be PETSC_NULL)

   Calling sequence of f:
$  f(TS ts,PetscReal t,Vec u,Vec u_t,Vec F,ctx);

+  t   - time at step/stage being solved
.  u   - state vector
.  u_t - time derivative of state vector
.  F   - function vector
-  ctx - [optional] user-defined context for matrix evaluation routine

   Important:
   The user MUST call either this routine, TSSetRHSFunction(), or TSSetMatrices().  This routine must be used when not solving an ODE.

   Level: beginner

.keywords: TS, timestep, set, DAE, Jacobian

.seealso: TSSetMatrices(), TSSetRHSFunction(), TSSetIJacobian()
@*/
PetscErrorCode  TSSetIFunction(TS ts,Vec res,TSIFunction f,void *ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (res) PetscValidHeaderSpecific(res,VEC_CLASSID,2);
  if (f)   ts->ops->ifunction = f;
  if (ctx) ts->funP           = ctx;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes,res,SNESTSFormFunction,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetIFunction"
/*@C
   TSGetIFunction - Returns the vector where the implicit residual is stored and the function/contex to compute it.

   Not Collective

   Input Parameter:
.  ts - the TS context

   Output Parameter:
+  r - vector to hold residual (or PETSC_NULL)
.  func - the function to compute residual (or PETSC_NULL)
-  ctx - the function context (or PETSC_NULL)

   Level: advanced

.keywords: TS, nonlinear, get, function

.seealso: TSSetIFunction(), SNESGetFunction()
@*/
PetscErrorCode TSGetIFunction(TS ts,Vec *r,TSIFunction *func,void **ctx)
{
  PetscErrorCode ierr;
  SNES snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESGetFunction(snes,r,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  if (func) *func = ts->ops->ifunction;
  if (ctx)  *ctx  = ts->funP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetRHSFunction"
/*@C
   TSGetRHSFunction - Returns the vector where the right hand side is stored and the function/context to compute it.

   Not Collective

   Input Parameter:
.  ts - the TS context

   Output Parameter:
+  r - vector to hold computed right hand side (or PETSC_NULL)
.  func - the function to compute right hand side (or PETSC_NULL)
-  ctx - the function context (or PETSC_NULL)

   Level: advanced

.keywords: TS, nonlinear, get, function

.seealso: TSSetRhsfunction(), SNESGetFunction()
@*/
PetscErrorCode TSGetRHSFunction(TS ts,Vec *r,TSRHSFunction *func,void **ctx)
{
  PetscErrorCode ierr;
  SNES snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESGetFunction(snes,r,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  if (func) *func = ts->ops->rhsfunction;
  if (ctx)  *ctx  = ts->funP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSSetIJacobian"
/*@C
   TSSetIJacobian - Set the function to compute the matrix dF/dU + a*dF/dU_t where F(t,U,U_t) is the function
        you provided with TSSetIFunction().  

   Logically Collective on TS

   Input Parameters:
+  ts  - the TS context obtained from TSCreate()
.  A   - Jacobian matrix
.  B   - preconditioning matrix for A (may be same as A)
.  f   - the Jacobian evaluation routine
-  ctx - user-defined context for private data for the Jacobian evaluation routine (may be PETSC_NULL)

   Calling sequence of f:
$  f(TS ts,PetscReal t,Vec U,Vec U_t,PetscReal a,Mat *A,Mat *B,MatStructure *flag,void *ctx);

+  t    - time at step/stage being solved
.  U    - state vector
.  U_t  - time derivative of state vector
.  a    - shift
.  A    - Jacobian of G(U) = F(t,U,W+a*U), equivalent to dF/dU + a*dF/dU_t
.  B    - preconditioning matrix for A, may be same as A
.  flag - flag indicating information about the preconditioner matrix
          structure (same as flag in KSPSetOperators())
-  ctx  - [optional] user-defined context for matrix evaluation routine

   Notes:
   The matrices A and B are exactly the matrices that are used by SNES for the nonlinear solve.

   The matrix dF/dU + a*dF/dU_t you provide turns out to be 
   the Jacobian of G(U) = F(t,U,W+a*U) where F(t,U,U_t) = 0 is the DAE to be solved.
   The time integrator internally approximates U_t by W+a*U where the positive "shift"
   a and vector W depend on the integration method, step size, and past states. For example with 
   the backward Euler method a = 1/dt and W = -a*U(previous timestep) so
   W + a*U = a*(U - U(previous timestep)) = (U - U(previous timestep))/dt

   Level: beginner

.keywords: TS, timestep, DAE, Jacobian

.seealso: TSSetIFunction(), TSSetRHSJacobian()

@*/
PetscErrorCode  TSSetIJacobian(TS ts,Mat A,Mat B,TSIJacobian f,void *ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (A) PetscValidHeaderSpecific(A,MAT_CLASSID,2);
  if (B) PetscValidHeaderSpecific(B,MAT_CLASSID,3);
  if (A) PetscCheckSameComm(ts,1,A,2);
  if (B) PetscCheckSameComm(ts,1,B,3);
  if (f)   ts->ops->ijacobian = f;
  if (ctx) ts->jacP           = ctx;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes,A,B,SNESTSFormJacobian,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSView"
/*@C
    TSView - Prints the TS data structure.

    Collective on TS

    Input Parameters:
+   ts - the TS context obtained from TSCreate()
-   viewer - visualization context

    Options Database Key:
.   -ts_view - calls TSView() at end of TSStep()

    Notes:
    The available visualization contexts include
+     PETSC_VIEWER_STDOUT_SELF - standard output (default)
-     PETSC_VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

    The user can open an alternative visualization context with
    PetscViewerASCIIOpen() - output to a specified file.

    Level: beginner

.keywords: TS, timestep, view

.seealso: PetscViewerASCIIOpen()
@*/
PetscErrorCode  TSView(TS ts,PetscViewer viewer)
{
  PetscErrorCode ierr;
  const TSType   type;
  PetscBool      iascii,isstring,isundials;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (!viewer) {
    ierr = PetscViewerASCIIGetStdout(((PetscObject)ts)->comm,&viewer);CHKERRQ(ierr);
  }
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_CLASSID,2);
  PetscCheckSameComm(ts,1,viewer,2);

  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERSTRING,&isstring);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscObjectPrintClassNamePrefixType((PetscObject)ts,viewer,"TS Object");CHKERRQ(ierr);
    if (ts->ops->view) {
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = (*ts->ops->view)(ts,viewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
    ierr = PetscViewerASCIIPrintf(viewer,"  maximum steps=%D\n",ts->max_steps);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  maximum time=%G\n",ts->max_time);CHKERRQ(ierr);
    if (ts->problem_type == TS_NONLINEAR) {
      ierr = PetscViewerASCIIPrintf(viewer,"  total number of nonlinear solver iterations=%D\n",ts->nonlinear_its);CHKERRQ(ierr);
    }
    ierr = PetscViewerASCIIPrintf(viewer,"  total number of linear solver iterations=%D\n",ts->linear_its);CHKERRQ(ierr);
  } else if (isstring) {
    ierr = TSGetType(ts,&type);CHKERRQ(ierr);
    ierr = PetscViewerStringSPrintf(viewer," %-7.7s",type);CHKERRQ(ierr);
  }
  ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)ts,TSSUNDIALS,&isundials);CHKERRQ(ierr);
  if (!isundials && ts->snes) {ierr = SNESView(ts->snes,viewer);CHKERRQ(ierr);}
  ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSSetApplicationContext"
/*@
   TSSetApplicationContext - Sets an optional user-defined context for 
   the timesteppers.

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
-  usrP - optional user context

   Level: intermediate

.keywords: TS, timestep, set, application, context

.seealso: TSGetApplicationContext()
@*/
PetscErrorCode  TSSetApplicationContext(TS ts,void *usrP)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ts->user = usrP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetApplicationContext"
/*@
    TSGetApplicationContext - Gets the user-defined context for the 
    timestepper.

    Not Collective

    Input Parameter:
.   ts - the TS context obtained from TSCreate()

    Output Parameter:
.   usrP - user context

    Level: intermediate

.keywords: TS, timestep, get, application, context

.seealso: TSSetApplicationContext()
@*/
PetscErrorCode  TSGetApplicationContext(TS ts,void *usrP)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  *(void**)usrP = ts->user;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetTimeStepNumber"
/*@
   TSGetTimeStepNumber - Gets the current number of timesteps.

   Not Collective

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameter:
.  iter - number steps so far

   Level: intermediate

.keywords: TS, timestep, get, iteration, number
@*/
PetscErrorCode  TSGetTimeStepNumber(TS ts,PetscInt* iter)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidIntPointer(iter,2);
  *iter = ts->steps;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetInitialTimeStep"
/*@
   TSSetInitialTimeStep - Sets the initial timestep to be used, 
   as well as the initial time.

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
.  initial_time - the initial time
-  time_step - the size of the timestep

   Level: intermediate

.seealso: TSSetTimeStep(), TSGetTimeStep()

.keywords: TS, set, initial, timestep
@*/
PetscErrorCode  TSSetInitialTimeStep(TS ts,PetscReal initial_time,PetscReal time_step)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ts->time_step         = time_step;
  ts->initial_time_step = time_step;
  ts->ptime             = initial_time;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetTimeStep"
/*@
   TSSetTimeStep - Allows one to reset the timestep at any time,
   useful for simple pseudo-timestepping codes.

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
-  time_step - the size of the timestep

   Level: intermediate

.seealso: TSSetInitialTimeStep(), TSGetTimeStep()

.keywords: TS, set, timestep
@*/
PetscErrorCode  TSSetTimeStep(TS ts,PetscReal time_step)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidLogicalCollectiveReal(ts,time_step,2);
  ts->time_step = time_step;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetTimeStep"
/*@
   TSGetTimeStep - Gets the current timestep size.

   Not Collective

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameter:
.  dt - the current timestep size

   Level: intermediate

.seealso: TSSetInitialTimeStep(), TSGetTimeStep()

.keywords: TS, get, timestep
@*/
PetscErrorCode  TSGetTimeStep(TS ts,PetscReal* dt)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidDoublePointer(dt,2);
  *dt = ts->time_step;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetSolution"
/*@
   TSGetSolution - Returns the solution at the present timestep. It
   is valid to call this routine inside the function that you are evaluating
   in order to move to the new timestep. This vector not changed until
   the solution at the next timestep has been calculated.

   Not Collective, but Vec returned is parallel if TS is parallel

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameter:
.  v - the vector containing the solution

   Level: intermediate

.seealso: TSGetTimeStep()

.keywords: TS, timestep, get, solution
@*/
PetscErrorCode  TSGetSolution(TS ts,Vec *v)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidPointer(v,2);
  *v = ts->vec_sol;
  PetscFunctionReturn(0);
}

/* ----- Routines to initialize and destroy a timestepper ---- */
#undef __FUNCT__  
#define __FUNCT__ "TSSetProblemType"
/*@
  TSSetProblemType - Sets the type of problem to be solved.

  Not collective

  Input Parameters:
+ ts   - The TS
- type - One of TS_LINEAR, TS_NONLINEAR where these types refer to problems of the forms
.vb
         U_t = A U    
         U_t = A(t) U 
         U_t = F(t,U) 
.ve

   Level: beginner

.keywords: TS, problem type
.seealso: TSSetUp(), TSProblemType, TS
@*/
PetscErrorCode  TSSetProblemType(TS ts, TSProblemType type) 
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);
  ts->problem_type = type;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetProblemType"
/*@C
  TSGetProblemType - Gets the type of problem to be solved.

  Not collective

  Input Parameter:
. ts   - The TS

  Output Parameter:
. type - One of TS_LINEAR, TS_NONLINEAR where these types refer to problems of the forms
.vb
         M U_t = A U
         M(t) U_t = A(t) U
         U_t = F(t,U)
.ve

   Level: beginner

.keywords: TS, problem type
.seealso: TSSetUp(), TSProblemType, TS
@*/
PetscErrorCode  TSGetProblemType(TS ts, TSProblemType *type) 
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);
  PetscValidIntPointer(type,2);
  *type = ts->problem_type;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetUp"
/*@
   TSSetUp - Sets up the internal data structures for the later use
   of a timestepper.  

   Collective on TS

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Notes:
   For basic use of the TS solvers the user need not explicitly call
   TSSetUp(), since these actions will automatically occur during
   the call to TSStep().  However, if one wishes to control this
   phase separately, TSSetUp() should be called after TSCreate()
   and optional routines of the form TSSetXXX(), but before TSStep().  

   Level: advanced

.keywords: TS, timestep, setup

.seealso: TSCreate(), TSStep(), TSDestroy()
@*/
PetscErrorCode  TSSetUp(TS ts)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (ts->setupcalled) PetscFunctionReturn(0);

  if (!((PetscObject)ts)->type_name) {
    ierr = TSSetType(ts,TSEULER);CHKERRQ(ierr);
  }

  if (!ts->vec_sol) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"Must call TSSetSolution() first");

  if (ts->ops->setup) {
    ierr = (*ts->ops->setup)(ts);CHKERRQ(ierr);
  }

  ts->setupcalled = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSReset"
/*@
   TSReset - Resets a TS context and removes any allocated Vecs and Mats.

   Collective on TS

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Level: beginner

.keywords: TS, timestep, reset

.seealso: TSCreate(), TSSetup(), TSDestroy()
@*/
PetscErrorCode  TSReset(TS ts)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (ts->ops->reset) {
    ierr = (*ts->ops->reset)(ts);CHKERRQ(ierr);
  }
  if (ts->snes) {ierr = SNESReset(ts->snes);CHKERRQ(ierr);}
  ierr = MatDestroy(&ts->Alhs);CHKERRQ(ierr);
  ierr = MatDestroy(&ts->Blhs);CHKERRQ(ierr);
  ierr = VecDestroy(&ts->vec_sol);CHKERRQ(ierr);
  if (ts->work) {ierr = VecDestroyVecs(ts->nwork,&ts->work);CHKERRQ(ierr);}
  ts->setupcalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSDestroy"
/*@
   TSDestroy - Destroys the timestepper context that was created
   with TSCreate().

   Collective on TS

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Level: beginner

.keywords: TS, timestepper, destroy

.seealso: TSCreate(), TSSetUp(), TSSolve()
@*/
PetscErrorCode  TSDestroy(TS *ts)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!*ts) PetscFunctionReturn(0);
  PetscValidHeaderSpecific((*ts),TS_CLASSID,1);
  if (--((PetscObject)(*ts))->refct > 0) {*ts = 0; PetscFunctionReturn(0);}

  ierr = TSReset((*ts));CHKERRQ(ierr);

  /* if memory was published with AMS then destroy it */
  ierr = PetscObjectDepublish((*ts));CHKERRQ(ierr);
  if ((*ts)->ops->destroy) {ierr = (*(*ts)->ops->destroy)((*ts));CHKERRQ(ierr);}

  ierr = SNESDestroy(&(*ts)->snes);CHKERRQ(ierr);
  ierr = DMDestroy(&(*ts)->dm);CHKERRQ(ierr);
  ierr = TSMonitorCancel((*ts));CHKERRQ(ierr);

  ierr = PetscHeaderDestroy(ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetSNES"
/*@
   TSGetSNES - Returns the SNES (nonlinear solver) associated with 
   a TS (timestepper) context. Valid only for nonlinear problems.

   Not Collective, but SNES is parallel if TS is parallel

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameter:
.  snes - the nonlinear solver context

   Notes:
   The user can then directly manipulate the SNES context to set various
   options, etc.  Likewise, the user can then extract and manipulate the 
   KSP, KSP, and PC contexts as well.

   TSGetSNES() does not work for integrators that do not use SNES; in
   this case TSGetSNES() returns PETSC_NULL in snes.

   Level: beginner

.keywords: timestep, get, SNES
@*/
PetscErrorCode  TSGetSNES(TS ts,SNES *snes)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidPointer(snes,2);
  if (!ts->snes) {
    ierr = SNESCreate(((PetscObject)ts)->comm,&ts->snes);CHKERRQ(ierr);
    ierr = PetscLogObjectParent(ts,ts->snes);CHKERRQ(ierr);
    ierr = PetscObjectIncrementTabLevel((PetscObject)ts->snes,(PetscObject)ts,1);CHKERRQ(ierr);
  }
  *snes = ts->snes;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSGetKSP"
/*@
   TSGetKSP - Returns the KSP (linear solver) associated with
   a TS (timestepper) context.

   Not Collective, but KSP is parallel if TS is parallel

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameter:
.  ksp - the nonlinear solver context

   Notes:
   The user can then directly manipulate the KSP context to set various
   options, etc.  Likewise, the user can then extract and manipulate the
   KSP and PC contexts as well.

   TSGetKSP() does not work for integrators that do not use KSP;
   in this case TSGetKSP() returns PETSC_NULL in ksp.

   Level: beginner

.keywords: timestep, get, KSP
@*/
PetscErrorCode  TSGetKSP(TS ts,KSP *ksp)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidPointer(ksp,2);
  if (!((PetscObject)ts)->type_name) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_NULL,"KSP is not created yet. Call TSSetType() first");
  if (ts->problem_type != TS_LINEAR) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"Linear only; use TSGetSNES()");
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESGetKSP(snes,ksp);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------- Routines to set solver parameters ---------- */

#undef __FUNCT__  
#define __FUNCT__ "TSGetDuration"
/*@
   TSGetDuration - Gets the maximum number of timesteps to use and 
   maximum time for iteration.

   Not Collective

   Input Parameters:
+  ts       - the TS context obtained from TSCreate()
.  maxsteps - maximum number of iterations to use, or PETSC_NULL
-  maxtime  - final time to iterate to, or PETSC_NULL

   Level: intermediate

.keywords: TS, timestep, get, maximum, iterations, time
@*/
PetscErrorCode  TSGetDuration(TS ts, PetscInt *maxsteps, PetscReal *maxtime)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);
  if (maxsteps) {
    PetscValidIntPointer(maxsteps,2);
    *maxsteps = ts->max_steps;
  }
  if (maxtime ) {
    PetscValidScalarPointer(maxtime,3);
    *maxtime  = ts->max_time;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetDuration"
/*@
   TSSetDuration - Sets the maximum number of timesteps to use and 
   maximum time for iteration.

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
.  maxsteps - maximum number of iterations to use
-  maxtime - final time to iterate to

   Options Database Keys:
.  -ts_max_steps <maxsteps> - Sets maxsteps
.  -ts_max_time <maxtime> - Sets maxtime

   Notes:
   The default maximum number of iterations is 5000. Default time is 5.0

   Level: intermediate

.keywords: TS, timestep, set, maximum, iterations
@*/
PetscErrorCode  TSSetDuration(TS ts,PetscInt maxsteps,PetscReal maxtime)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidLogicalCollectiveInt(ts,maxsteps,2);
  PetscValidLogicalCollectiveReal(ts,maxtime,2);
  ts->max_steps = maxsteps;
  ts->max_time  = maxtime;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetSolution"
/*@
   TSSetSolution - Sets the initial solution vector
   for use by the TS routines.

   Logically Collective on TS and Vec

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
-  x - the solution vector

   Level: beginner

.keywords: TS, timestep, set, solution, initial conditions
@*/
PetscErrorCode  TSSetSolution(TS ts,Vec x)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidHeaderSpecific(x,VEC_CLASSID,2);
  ierr = PetscObjectReference((PetscObject)x);CHKERRQ(ierr);
  ierr = VecDestroy(&ts->vec_sol);CHKERRQ(ierr);
  ts->vec_sol = x;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSSetPreStep"
/*@C
  TSSetPreStep - Sets the general-purpose function
  called once at the beginning of each time step.

  Logically Collective on TS

  Input Parameters:
+ ts   - The TS context obtained from TSCreate()
- func - The function

  Calling sequence of func:
. func (TS ts);

  Level: intermediate

.keywords: TS, timestep
@*/
PetscErrorCode  TSSetPreStep(TS ts, PetscErrorCode (*func)(TS))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);
  ts->ops->prestep = func;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSPreStep"
/*@C
  TSPreStep - Runs the user-defined pre-step function.

  Collective on TS

  Input Parameters:
. ts   - The TS context obtained from TSCreate()

  Notes:
  TSPreStep() is typically used within time stepping implementations,
  so most users would not generally call this routine themselves.

  Level: developer

.keywords: TS, timestep
@*/
PetscErrorCode  TSPreStep(TS ts)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (ts->ops->prestep) {
    PetscStackPush("TS PreStep function");
    ierr = (*ts->ops->prestep)(ts);CHKERRQ(ierr);
    PetscStackPop;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSSetPostStep"
/*@C
  TSSetPostStep - Sets the general-purpose function
  called once at the end of each time step.

  Logically Collective on TS

  Input Parameters:
+ ts   - The TS context obtained from TSCreate()
- func - The function

  Calling sequence of func:
. func (TS ts);

  Level: intermediate

.keywords: TS, timestep
@*/
PetscErrorCode  TSSetPostStep(TS ts, PetscErrorCode (*func)(TS))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);
  ts->ops->poststep = func;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSPostStep"
/*@C
  TSPostStep - Runs the user-defined post-step function.

  Collective on TS

  Input Parameters:
. ts   - The TS context obtained from TSCreate()

  Notes:
  TSPostStep() is typically used within time stepping implementations,
  so most users would not generally call this routine themselves.

  Level: developer

.keywords: TS, timestep
@*/
PetscErrorCode  TSPostStep(TS ts)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (ts->ops->poststep) {
    PetscStackPush("TS PostStep function");
    ierr = (*ts->ops->poststep)(ts);CHKERRQ(ierr);
    PetscStackPop;
  }
  PetscFunctionReturn(0);
}

/* ------------ Routines to set performance monitoring options ----------- */

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorSet"
/*@C
   TSMonitorSet - Sets an ADDITIONAL function that is to be used at every
   timestep to display the iteration's  progress.   

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
.  func - monitoring routine
.  mctx - [optional] user-defined context for private data for the 
             monitor routine (use PETSC_NULL if no context is desired)
-  monitordestroy - [optional] routine that frees monitor context
          (may be PETSC_NULL)

   Calling sequence of func:
$    int func(TS ts,PetscInt steps,PetscReal time,Vec x,void *mctx)

+    ts - the TS context
.    steps - iteration number
.    time - current time
.    x - current iterate
-    mctx - [optional] monitoring context

   Notes:
   This routine adds an additional monitor to the list of monitors that 
   already has been loaded.

   Fortran notes: Only a single monitor function can be set for each TS object

   Level: intermediate

.keywords: TS, timestep, set, monitor

.seealso: TSMonitorDefault(), TSMonitorCancel()
@*/
PetscErrorCode  TSMonitorSet(TS ts,PetscErrorCode (*monitor)(TS,PetscInt,PetscReal,Vec,void*),void *mctx,PetscErrorCode (*mdestroy)(void**))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  if (ts->numbermonitors >= MAXTSMONITORS) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_OUTOFRANGE,"Too many monitors set");
  ts->monitor[ts->numbermonitors]           = monitor;
  ts->mdestroy[ts->numbermonitors]          = mdestroy;
  ts->monitorcontext[ts->numbermonitors++]  = (void*)mctx;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorCancel"
/*@C
   TSMonitorCancel - Clears all the monitors that have been set on a time-step object.   

   Logically Collective on TS

   Input Parameters:
.  ts - the TS context obtained from TSCreate()

   Notes:
   There is no way to remove a single, specific monitor.

   Level: intermediate

.keywords: TS, timestep, set, monitor

.seealso: TSMonitorDefault(), TSMonitorSet()
@*/
PetscErrorCode  TSMonitorCancel(TS ts)
{
  PetscErrorCode ierr;
  PetscInt       i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  for (i=0; i<ts->numbermonitors; i++) {
    if (ts->mdestroy[i]) {
      ierr = (*ts->mdestroy[i])(&ts->monitorcontext[i]);CHKERRQ(ierr);
    }
  }
  ts->numbermonitors = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorDefault"
/*@
   TSMonitorDefault - Sets the Default monitor

   Level: intermediate

.keywords: TS, set, monitor

.seealso: TSMonitorDefault(), TSMonitorSet()
@*/
PetscErrorCode TSMonitorDefault(TS ts,PetscInt step,PetscReal ptime,Vec v,void *dummy)
{
  PetscErrorCode ierr;
  PetscViewer    viewer = dummy ? (PetscViewer) dummy : PETSC_VIEWER_STDOUT_(((PetscObject)ts)->comm);

  PetscFunctionBegin;
  ierr = PetscViewerASCIIAddTab(viewer,((PetscObject)ts)->tablevel);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"%D TS dt %G time %G\n",step,ts->time_step,ptime);CHKERRQ(ierr);
  ierr = PetscViewerASCIISubtractTab(viewer,((PetscObject)ts)->tablevel);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSStep"
/*@
   TSStep - Steps the requested number of timesteps.

   Collective on TS

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameters:
+  steps - number of iterations until termination
-  ptime - time until termination

   Level: beginner

.keywords: TS, timestep, solve

.seealso: TSCreate(), TSSetUp(), TSDestroy()
@*/
PetscErrorCode  TSStep(TS ts,PetscInt *steps,PetscReal *ptime)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID,1);

  ierr = TSSetUp(ts);CHKERRQ(ierr);

  ierr = PetscLogEventBegin(TS_Step, ts, 0, 0, 0);CHKERRQ(ierr);
  ierr = (*ts->ops->step)(ts, steps, ptime);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(TS_Step, ts, 0, 0, 0);CHKERRQ(ierr);

  if (!PetscPreLoadingOn) {
    ierr = TSViewFromOptions(ts,((PetscObject)ts)->name);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSolve"
/*@
   TSSolve - Steps the requested number of timesteps.

   Collective on TS

   Input Parameter:
+  ts - the TS context obtained from TSCreate()
-  x - the solution vector, or PETSC_NULL if it was set with TSSetSolution()

   Level: beginner

.keywords: TS, timestep, solve

.seealso: TSCreate(), TSSetSolution(), TSStep()
@*/
PetscErrorCode  TSSolve(TS ts, Vec x)
{
  PetscInt       steps;
  PetscReal      ptime;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  /* set solution vector if provided */
  if (x) { ierr = TSSetSolution(ts, x); CHKERRQ(ierr); }
  /* reset time step and iteration counters */
  ts->steps = 0; ts->linear_its = 0; ts->nonlinear_its = 0;
  /* steps the requested number of timesteps. */
  ierr = TSStep(ts, &steps, &ptime);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSMonitor"
/*
     Runs the user provided monitor routines, if they exists.
*/
PetscErrorCode TSMonitor(TS ts,PetscInt step,PetscReal ptime,Vec x)
{
  PetscErrorCode ierr;
  PetscInt       i,n = ts->numbermonitors;

  PetscFunctionBegin;
  for (i=0; i<n; i++) {
    ierr = (*ts->monitor[i])(ts,step,ptime,x,ts->monitorcontext[i]);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorLGCreate"
/*@C
   TSMonitorLGCreate - Creates a line graph context for use with 
   TS to monitor convergence of preconditioned residual norms.

   Collective on TS

   Input Parameters:
+  host - the X display to open, or null for the local machine
.  label - the title to put in the title bar
.  x, y - the screen coordinates of the upper left coordinate of the window
-  m, n - the screen width and height in pixels

   Output Parameter:
.  draw - the drawing context

   Options Database Key:
.  -ts_monitor_draw - automatically sets line graph monitor

   Notes: 
   Use TSMonitorLGDestroy() to destroy this line graph, not PetscDrawLGDestroy().

   Level: intermediate

.keywords: TS, monitor, line graph, residual, seealso

.seealso: TSMonitorLGDestroy(), TSMonitorSet()

@*/
PetscErrorCode  TSMonitorLGCreate(const char host[],const char label[],int x,int y,int m,int n,PetscDrawLG *draw)
{
  PetscDraw      win;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscDrawCreate(PETSC_COMM_SELF,host,label,x,y,m,n,&win);CHKERRQ(ierr);
  ierr = PetscDrawSetType(win,PETSC_DRAW_X);CHKERRQ(ierr);
  ierr = PetscDrawLGCreate(win,1,draw);CHKERRQ(ierr);
  ierr = PetscDrawLGIndicateDataPoints(*draw);CHKERRQ(ierr);

  ierr = PetscLogObjectParent(*draw,win);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorLG"
PetscErrorCode TSMonitorLG(TS ts,PetscInt n,PetscReal ptime,Vec v,void *monctx)
{
  PetscDrawLG    lg = (PetscDrawLG) monctx;
  PetscReal      x,y = ptime;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!monctx) {
    MPI_Comm    comm;
    PetscViewer viewer;

    ierr   = PetscObjectGetComm((PetscObject)ts,&comm);CHKERRQ(ierr);
    viewer = PETSC_VIEWER_DRAW_(comm);
    ierr   = PetscViewerDrawGetDrawLG(viewer,0,&lg);CHKERRQ(ierr);
  }

  if (!n) {ierr = PetscDrawLGReset(lg);CHKERRQ(ierr);}
  x = (PetscReal)n;
  ierr = PetscDrawLGAddPoint(lg,&x,&y);CHKERRQ(ierr);
  if (n < 20 || (n % 5)) {
    ierr = PetscDrawLGDraw(lg);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
} 

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorLGDestroy" 
/*@C
   TSMonitorLGDestroy - Destroys a line graph context that was created 
   with TSMonitorLGCreate().

   Collective on PetscDrawLG

   Input Parameter:
.  draw - the drawing context

   Level: intermediate

.keywords: TS, monitor, line graph, destroy

.seealso: TSMonitorLGCreate(),  TSMonitorSet(), TSMonitorLG();
@*/
PetscErrorCode  TSMonitorLGDestroy(PetscDrawLG *drawlg)
{
  PetscDraw      draw;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscDrawLGGetDraw(*drawlg,&draw);CHKERRQ(ierr);
  ierr = PetscDrawDestroy(&draw);CHKERRQ(ierr);
  ierr = PetscDrawLGDestroy(drawlg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetTime"
/*@
   TSGetTime - Gets the current time.

   Not Collective

   Input Parameter:
.  ts - the TS context obtained from TSCreate()

   Output Parameter:
.  t  - the current time

   Level: beginner

.seealso: TSSetInitialTimeStep(), TSGetTimeStep()

.keywords: TS, get, time
@*/
PetscErrorCode  TSGetTime(TS ts,PetscReal* t)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidDoublePointer(t,2);
  *t = ts->ptime;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetTime"
/*@
   TSSetTime - Allows one to reset the time.

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context obtained from TSCreate()
-  time - the time

   Level: intermediate

.seealso: TSGetTime(), TSSetDuration()

.keywords: TS, set, time
@*/
PetscErrorCode  TSSetTime(TS ts, PetscReal t) 
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidLogicalCollectiveReal(ts,t,2);
  ts->ptime = t;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSSetOptionsPrefix" 
/*@C
   TSSetOptionsPrefix - Sets the prefix used for searching for all
   TS options in the database.

   Logically Collective on TS

   Input Parameter:
+  ts     - The TS context
-  prefix - The prefix to prepend to all option names

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Level: advanced

.keywords: TS, set, options, prefix, database

.seealso: TSSetFromOptions()

@*/
PetscErrorCode  TSSetOptionsPrefix(TS ts,const char prefix[])
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ierr = PetscObjectSetOptionsPrefix((PetscObject)ts,prefix);CHKERRQ(ierr);
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetOptionsPrefix(snes,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "TSAppendOptionsPrefix" 
/*@C
   TSAppendOptionsPrefix - Appends to the prefix used for searching for all
   TS options in the database.

   Logically Collective on TS

   Input Parameter:
+  ts     - The TS context
-  prefix - The prefix to prepend to all option names

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Level: advanced

.keywords: TS, append, options, prefix, database

.seealso: TSGetOptionsPrefix()

@*/
PetscErrorCode  TSAppendOptionsPrefix(TS ts,const char prefix[])
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ierr = PetscObjectAppendOptionsPrefix((PetscObject)ts,prefix);CHKERRQ(ierr);
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESAppendOptionsPrefix(snes,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSGetOptionsPrefix"
/*@C
   TSGetOptionsPrefix - Sets the prefix used for searching for all
   TS options in the database.

   Not Collective

   Input Parameter:
.  ts - The TS context

   Output Parameter:
.  prefix - A pointer to the prefix string used

   Notes: On the fortran side, the user should pass in a string 'prifix' of
   sufficient length to hold the prefix.

   Level: intermediate

.keywords: TS, get, options, prefix, database

.seealso: TSAppendOptionsPrefix()
@*/
PetscErrorCode  TSGetOptionsPrefix(TS ts,const char *prefix[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  PetscValidPointer(prefix,2);
  ierr = PetscObjectGetOptionsPrefix((PetscObject)ts,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSGetRHSJacobian"
/*@C
   TSGetRHSJacobian - Returns the Jacobian J at the present timestep.

   Not Collective, but parallel objects are returned if TS is parallel

   Input Parameter:
.  ts  - The TS context obtained from TSCreate()

   Output Parameters:
+  J   - The Jacobian J of F, where U_t = F(U,t)
.  M   - The preconditioner matrix, usually the same as J
.  func - Function to compute the Jacobian of the RHS
-  ctx - User-defined context for Jacobian evaluation routine

   Notes: You can pass in PETSC_NULL for any return argument you do not need.

   Level: intermediate

.seealso: TSGetTimeStep(), TSGetMatrices(), TSGetTime(), TSGetTimeStepNumber()

.keywords: TS, timestep, get, matrix, Jacobian
@*/
PetscErrorCode  TSGetRHSJacobian(TS ts,Mat *J,Mat *M,TSRHSJacobian *func,void **ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESGetJacobian(snes,J,M,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  if (func) *func = ts->ops->rhsjacobian;
  if (ctx) *ctx = ts->jacP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "TSGetIJacobian"
/*@C
   TSGetIJacobian - Returns the implicit Jacobian at the present timestep.

   Not Collective, but parallel objects are returned if TS is parallel

   Input Parameter:
.  ts  - The TS context obtained from TSCreate()

   Output Parameters:
+  A   - The Jacobian of F(t,U,U_t)
.  B   - The preconditioner matrix, often the same as A
.  f   - The function to compute the matrices
- ctx - User-defined context for Jacobian evaluation routine

   Notes: You can pass in PETSC_NULL for any return argument you do not need.

   Level: advanced

.seealso: TSGetTimeStep(), TSGetRHSJacobian(), TSGetMatrices(), TSGetTime(), TSGetTimeStepNumber()

.keywords: TS, timestep, get, matrix, Jacobian
@*/
PetscErrorCode  TSGetIJacobian(TS ts,Mat *A,Mat *B,TSIJacobian *f,void **ctx)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESGetJacobian(snes,A,B,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  if (f) *f = ts->ops->ijacobian;
  if (ctx) *ctx = ts->jacP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSMonitorSolution"
/*@C
   TSMonitorSolution - Monitors progress of the TS solvers by calling 
   VecView() for the solution at each timestep

   Collective on TS

   Input Parameters:
+  ts - the TS context
.  step - current time-step
.  ptime - current time
-  dummy - either a viewer or PETSC_NULL

   Level: intermediate

.keywords: TS,  vector, monitor, view

.seealso: TSMonitorSet(), TSMonitorDefault(), VecView()
@*/
PetscErrorCode  TSMonitorSolution(TS ts,PetscInt step,PetscReal ptime,Vec x,void *dummy)
{
  PetscErrorCode ierr;
  PetscViewer    viewer = (PetscViewer) dummy;

  PetscFunctionBegin;
  if (!dummy) {
    viewer = PETSC_VIEWER_DRAW_(((PetscObject)ts)->comm);
  }
  ierr = VecView(x,viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSSetDM"
/*@
   TSSetDM - Sets the DM that may be used by some preconditioners

   Logically Collective on TS and DM

   Input Parameters:
+  ts - the preconditioner context
-  dm - the dm

   Level: intermediate


.seealso: TSGetDM(), SNESSetDM(), SNESGetDM()
@*/
PetscErrorCode  TSSetDM(TS ts,DM dm)
{
  PetscErrorCode ierr;
  SNES           snes;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  ierr = PetscObjectReference((PetscObject)dm);CHKERRQ(ierr);
  ierr = DMDestroy(&ts->dm);CHKERRQ(ierr);
  ts->dm = dm;
  ierr = TSGetSNES(ts,&snes);CHKERRQ(ierr);
  ierr = SNESSetDM(snes,dm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSGetDM"
/*@
   TSGetDM - Gets the DM that may be used by some preconditioners

   Not Collective

   Input Parameter:
. ts - the preconditioner context

   Output Parameter:
.  dm - the dm

   Level: intermediate


.seealso: TSSetDM(), SNESSetDM(), SNESGetDM()
@*/
PetscErrorCode  TSGetDM(TS ts,DM *dm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_CLASSID,1);
  *dm = ts->dm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNESTSFormFunction"
/*@
   SNESTSFormFunction - Function to evaluate nonlinear residual

   Logically Collective on SNES

   Input Parameter:
+ snes - nonlinear solver
. X - the current state at which to evaluate the residual
- ctx - user context, must be a TS

   Output Parameter:
. F - the nonlinear residual

   Notes:
   This function is not normally called by users and is automatically registered with the SNES used by TS.
   It is most frequently passed to MatFDColoringSetFunction().

   Level: advanced

.seealso: SNESSetFunction(), MatFDColoringSetFunction()
@*/
PetscErrorCode  SNESTSFormFunction(SNES snes,Vec X,Vec F,void *ctx)
{
  TS ts = (TS)ctx;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,SNES_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscValidHeaderSpecific(F,VEC_CLASSID,3);
  PetscValidHeaderSpecific(ts,TS_CLASSID,4);
  ierr = (ts->ops->snesfunction)(snes,X,F,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SNESTSFormJacobian"
/*@
   SNESTSFormJacobian - Function to evaluate the Jacobian

   Collective on SNES

   Input Parameter:
+ snes - nonlinear solver
. X - the current state at which to evaluate the residual
- ctx - user context, must be a TS

   Output Parameter:
+ A - the Jacobian
. B - the preconditioning matrix (may be the same as A)
- flag - indicates any structure change in the matrix

   Notes:
   This function is not normally called by users and is automatically registered with the SNES used by TS.

   Level: developer

.seealso: SNESSetJacobian()
@*/
PetscErrorCode  SNESTSFormJacobian(SNES snes,Vec X,Mat *A,Mat *B,MatStructure *flag,void *ctx)
{
  TS ts = (TS)ctx;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,SNES_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscValidPointer(A,3);
  PetscValidHeaderSpecific(*A,MAT_CLASSID,3);
  PetscValidPointer(B,4);
  PetscValidHeaderSpecific(*B,MAT_CLASSID,4);
  PetscValidPointer(flag,5);
  PetscValidHeaderSpecific(ts,TS_CLASSID,6);
  ierr = (ts->ops->snesjacobian)(snes,X,A,B,flag,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_MATLAB_ENGINE)
#include <mex.h>

typedef struct {char *funcname; mxArray *ctx;} TSMatlabContext;

#undef __FUNCT__  
#define __FUNCT__ "TSComputeFunction_Matlab"
/*
   TSComputeFunction_Matlab - Calls the function that has been set with
                         TSSetFunctionMatlab().  

   Collective on TS

   Input Parameters:
+  snes - the TS context
-  x - input vector

   Output Parameter:
.  y - function vector, as set by TSSetFunction()

   Notes:
   TSComputeFunction() is typically used within nonlinear solvers
   implementations, so most users would not generally call this routine
   themselves.

   Level: developer

.keywords: TS, nonlinear, compute, function

.seealso: TSSetFunction(), TSGetFunction()
*/
PetscErrorCode  TSComputeFunction_Matlab(TS snes,PetscReal time,Vec x,Vec xdot,Vec y, void *ctx)
{
  PetscErrorCode   ierr;
  TSMatlabContext *sctx = (TSMatlabContext *)ctx;
  int              nlhs = 1,nrhs = 7;
  mxArray	   *plhs[1],*prhs[7];
  long long int    lx = 0,lxdot = 0,ly = 0,ls = 0;
      
  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,TS_CLASSID,1);
  PetscValidHeaderSpecific(x,VEC_CLASSID,3);
  PetscValidHeaderSpecific(xdot,VEC_CLASSID,4);
  PetscValidHeaderSpecific(y,VEC_CLASSID,5);
  PetscCheckSameComm(snes,1,x,3);
  PetscCheckSameComm(snes,1,y,5);

  ierr = PetscMemcpy(&ls,&snes,sizeof(snes));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lx,&x,sizeof(x));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lxdot,&xdot,sizeof(xdot));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&ly,&y,sizeof(x));CHKERRQ(ierr); 
  prhs[0] =  mxCreateDoubleScalar((double)ls);
  prhs[1] =  mxCreateDoubleScalar(time);
  prhs[2] =  mxCreateDoubleScalar((double)lx);
  prhs[3] =  mxCreateDoubleScalar((double)lxdot);
  prhs[4] =  mxCreateDoubleScalar((double)ly);
  prhs[5] =  mxCreateString(sctx->funcname);
  prhs[6] =  sctx->ctx;
  ierr    =  mexCallMATLAB(nlhs,plhs,nrhs,prhs,"PetscTSComputeFunctionInternal");CHKERRQ(ierr);
  ierr    =  mxGetScalar(plhs[0]);CHKERRQ(ierr);
  mxDestroyArray(prhs[0]);
  mxDestroyArray(prhs[1]);
  mxDestroyArray(prhs[2]);
  mxDestroyArray(prhs[3]);
  mxDestroyArray(prhs[4]);
  mxDestroyArray(prhs[5]);
  mxDestroyArray(plhs[0]);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSSetFunctionMatlab"
/*
   TSSetFunctionMatlab - Sets the function evaluation routine and function 
   vector for use by the TS routines in solving ODEs
   equations from MATLAB. Here the function is a string containing the name of a MATLAB function

   Logically Collective on TS

   Input Parameters:
+  ts - the TS context
-  func - function evaluation routine

   Calling sequence of func:
$    func (TS ts,PetscReal time,Vec x,Vec xdot,Vec f,void *ctx);

   Level: beginner

.keywords: TS, nonlinear, set, function

.seealso: TSGetFunction(), TSComputeFunction(), TSSetJacobian(), TSSetFunction()
*/
PetscErrorCode  TSSetFunctionMatlab(TS snes,const char *func,mxArray *ctx)
{
  PetscErrorCode  ierr;
  TSMatlabContext *sctx;

  PetscFunctionBegin;
  /* currently sctx is memory bleed */
  ierr = PetscMalloc(sizeof(TSMatlabContext),&sctx);CHKERRQ(ierr);
  ierr = PetscStrallocpy(func,&sctx->funcname);CHKERRQ(ierr);
  /* 
     This should work, but it doesn't 
  sctx->ctx = ctx; 
  mexMakeArrayPersistent(sctx->ctx);
  */
  sctx->ctx = mxDuplicateArray(ctx);
  ierr = TSSetIFunction(snes,TSComputeFunction_Matlab,sctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSComputeJacobian_Matlab"
/*
   TSComputeJacobian_Matlab - Calls the function that has been set with
                         TSSetJacobianMatlab().  

   Collective on TS

   Input Parameters:
+  snes - the TS context
.  x - input vector
.  A, B - the matrices
-  ctx - user context

   Output Parameter:
.  flag - structure of the matrix

   Level: developer

.keywords: TS, nonlinear, compute, function

.seealso: TSSetFunction(), TSGetFunction()
@*/
PetscErrorCode  TSComputeJacobian_Matlab(TS snes,PetscReal time,Vec x,Vec xdot,PetscReal shift,Mat *A,Mat *B,MatStructure *flag, void *ctx)
{
  PetscErrorCode  ierr;
  TSMatlabContext *sctx = (TSMatlabContext *)ctx;
  int             nlhs = 2,nrhs = 9;
  mxArray	  *plhs[2],*prhs[9];
  long long int   lx = 0,lxdot = 0,lA = 0,ls = 0, lB = 0;
      
  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,TS_CLASSID,1);
  PetscValidHeaderSpecific(x,VEC_CLASSID,3);

  /* call Matlab function in ctx with arguments x and y */

  ierr = PetscMemcpy(&ls,&snes,sizeof(snes));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lx,&x,sizeof(x));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lxdot,&xdot,sizeof(x));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lA,A,sizeof(x));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lB,B,sizeof(x));CHKERRQ(ierr); 
  prhs[0] =  mxCreateDoubleScalar((double)ls);
  prhs[1] =  mxCreateDoubleScalar((double)time);
  prhs[2] =  mxCreateDoubleScalar((double)lx);
  prhs[3] =  mxCreateDoubleScalar((double)lxdot);
  prhs[4] =  mxCreateDoubleScalar((double)shift);
  prhs[5] =  mxCreateDoubleScalar((double)lA);
  prhs[6] =  mxCreateDoubleScalar((double)lB);
  prhs[7] =  mxCreateString(sctx->funcname);
  prhs[8] =  sctx->ctx;
  ierr    =  mexCallMATLAB(nlhs,plhs,nrhs,prhs,"PetscTSComputeJacobianInternal");CHKERRQ(ierr);
  ierr    =  mxGetScalar(plhs[0]);CHKERRQ(ierr);
  *flag   =  (MatStructure) mxGetScalar(plhs[1]);CHKERRQ(ierr);
  mxDestroyArray(prhs[0]);
  mxDestroyArray(prhs[1]);
  mxDestroyArray(prhs[2]);
  mxDestroyArray(prhs[3]);
  mxDestroyArray(prhs[4]);
  mxDestroyArray(prhs[5]);
  mxDestroyArray(prhs[6]);
  mxDestroyArray(prhs[7]);
  mxDestroyArray(plhs[0]);
  mxDestroyArray(plhs[1]);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSSetJacobianMatlab"
/*
   TSSetJacobianMatlab - Sets the Jacobian function evaluation routine and two empty Jacobian matrices
   vector for use by the TS routines in solving ODEs from MATLAB. Here the function is a string containing the name of a MATLAB function

   Logically Collective on TS

   Input Parameters:
+  snes - the TS context
.  A,B - Jacobian matrices
.  func - function evaluation routine
-  ctx - user context

   Calling sequence of func:
$    flag = func (TS snes,PetscReal time,Vec x,Vec xdot,Mat A,Mat B,void *ctx);


   Level: developer

.keywords: TS, nonlinear, set, function

.seealso: TSGetFunction(), TSComputeFunction(), TSSetJacobian(), TSSetFunction()
*/
PetscErrorCode  TSSetJacobianMatlab(TS snes,Mat A,Mat B,const char *func,mxArray *ctx)
{
  PetscErrorCode    ierr;
  TSMatlabContext *sctx;

  PetscFunctionBegin;
  /* currently sctx is memory bleed */
  ierr = PetscMalloc(sizeof(TSMatlabContext),&sctx);CHKERRQ(ierr);
  ierr = PetscStrallocpy(func,&sctx->funcname);CHKERRQ(ierr);
  /* 
     This should work, but it doesn't 
  sctx->ctx = ctx; 
  mexMakeArrayPersistent(sctx->ctx);
  */
  sctx->ctx = mxDuplicateArray(ctx);
  ierr = TSSetIJacobian(snes,A,B,TSComputeJacobian_Matlab,sctx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSMonitor_Matlab"
/*
   TSMonitor_Matlab - Calls the function that has been set with TSMonitorSetMatlab().  

   Collective on TS

.seealso: TSSetFunction(), TSGetFunction()
@*/
PetscErrorCode  TSMonitor_Matlab(TS snes,PetscInt it, PetscReal time,Vec x, void *ctx)
{
  PetscErrorCode  ierr;
  TSMatlabContext *sctx = (TSMatlabContext *)ctx;
  int             nlhs = 1,nrhs = 6;
  mxArray	  *plhs[1],*prhs[6];
  long long int   lx = 0,ls = 0;
      
  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes,TS_CLASSID,1);
  PetscValidHeaderSpecific(x,VEC_CLASSID,4);

  ierr = PetscMemcpy(&ls,&snes,sizeof(snes));CHKERRQ(ierr); 
  ierr = PetscMemcpy(&lx,&x,sizeof(x));CHKERRQ(ierr); 
  prhs[0] =  mxCreateDoubleScalar((double)ls);
  prhs[1] =  mxCreateDoubleScalar((double)it);
  prhs[2] =  mxCreateDoubleScalar((double)time);
  prhs[3] =  mxCreateDoubleScalar((double)lx);
  prhs[4] =  mxCreateString(sctx->funcname);
  prhs[5] =  sctx->ctx;
  ierr    =  mexCallMATLAB(nlhs,plhs,nrhs,prhs,"PetscTSMonitorInternal");CHKERRQ(ierr);
  ierr    =  mxGetScalar(plhs[0]);CHKERRQ(ierr);
  mxDestroyArray(prhs[0]);
  mxDestroyArray(prhs[1]);
  mxDestroyArray(prhs[2]);
  mxDestroyArray(prhs[3]);
  mxDestroyArray(prhs[4]);
  mxDestroyArray(plhs[0]);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSMonitorSetMatlab"
/*
   TSMonitorSetMatlab - Sets the monitor function from Matlab

   Level: developer

.keywords: TS, nonlinear, set, function

.seealso: TSGetFunction(), TSComputeFunction(), TSSetJacobian(), TSSetFunction()
*/
PetscErrorCode  TSMonitorSetMatlab(TS snes,const char *func,mxArray *ctx)
{
  PetscErrorCode    ierr;
  TSMatlabContext *sctx;

  PetscFunctionBegin;
  /* currently sctx is memory bleed */
  ierr = PetscMalloc(sizeof(TSMatlabContext),&sctx);CHKERRQ(ierr);
  ierr = PetscStrallocpy(func,&sctx->funcname);CHKERRQ(ierr);
  /* 
     This should work, but it doesn't 
  sctx->ctx = ctx; 
  mexMakeArrayPersistent(sctx->ctx);
  */
  sctx->ctx = mxDuplicateArray(ctx);
  ierr = TSMonitorSet(snes,TSMonitor_Matlab,sctx,PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#endif
