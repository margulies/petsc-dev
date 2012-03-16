#include <private/linesearchimpl.h> /*I "petscsnes.h" I*/

PetscBool  PetscLineSearchRegisterAllCalled = PETSC_FALSE;
PetscFList PetscLineSearchList              = PETSC_NULL;

PetscClassId   PETSCLINESEARCH_CLASSID;
PetscLogEvent  PetscLineSearch_Apply;

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchCreate"
/*@
   PetscLineSearchCreate - Creates the line search.

   Collective on LineSearch

   Input Parameters:
.  comm - MPI communicator for the line search

   Output Parameters:
.  outlinesearch - the line search instance.

   Level: Beginner

   .keywords: LineSearch, Create

   .seealso: LineSearchDestroy()
@*/

PetscErrorCode PetscLineSearchCreate(MPI_Comm comm, PetscLineSearch *outlinesearch) {
  PetscErrorCode      ierr;
  PetscLineSearch     linesearch;
  PetscFunctionBegin;
  PetscValidPointer(outlinesearch,2);
  *outlinesearch = PETSC_NULL;
  ierr = PetscHeaderCreate(linesearch,_p_LineSearch,struct _LineSearchOps,PETSCLINESEARCH_CLASSID, 0,
                           "PetscLineSearch","Line-search method","PetscLineSearch",comm,PetscLineSearchDestroy,PetscLineSearchView);CHKERRQ(ierr);

  linesearch->ops->precheckstep = PETSC_NULL;
  linesearch->ops->postcheckstep = PETSC_NULL;

  linesearch->vec_sol_new   = PETSC_NULL;
  linesearch->vec_func_new  = PETSC_NULL;
  linesearch->vec_sol       = PETSC_NULL;
  linesearch->vec_func      = PETSC_NULL;
  linesearch->vec_update    = PETSC_NULL;

  linesearch->lambda        = 1.0;
  linesearch->fnorm         = 1.0;
  linesearch->ynorm         = 1.0;
  linesearch->xnorm         = 1.0;
  linesearch->success       = PETSC_TRUE;
  linesearch->norms         = PETSC_TRUE;
  linesearch->keeplambda    = PETSC_FALSE;
  linesearch->damping       = 1.0;
  linesearch->maxstep       = 1e8;
  linesearch->steptol       = 1e-12;
  linesearch->rtol          = 1e-8;
  linesearch->atol          = 1e-15;
  linesearch->ltol          = 1e-8;
  linesearch->precheckctx   = PETSC_NULL;
  linesearch->postcheckctx  = PETSC_NULL;
  linesearch->max_its       = 3;
  linesearch->setupcalled   = PETSC_FALSE;
  *outlinesearch            = linesearch;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetUp"
/*@
   PetscLineSearchSetUp - Prepares the line search for being applied.

   Collective on LineSearch

   Input Parameters:
.  linesearch - The LineSearch instance.

   Level: Intermediate

   .keywords: PetscLineSearch, SetUp

   .seealso: PetscLineSearchReset()
@*/

PetscErrorCode PetscLineSearchSetUp(PetscLineSearch linesearch) {
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if (!((PetscObject)linesearch)->type_name) {
    ierr = PetscLineSearchSetType(linesearch,PETSCLINESEARCHBASIC);CHKERRQ(ierr);
  }
  if (!linesearch->setupcalled) {
    if (!linesearch->vec_sol_new) {
      ierr = VecDuplicate(linesearch->vec_sol, &linesearch->vec_sol_new);CHKERRQ(ierr);
    }
    if (!linesearch->vec_func_new) {
      ierr = VecDuplicate(linesearch->vec_sol, &linesearch->vec_func_new);CHKERRQ(ierr);
    }
    if (linesearch->ops->setup) {
      ierr = (*linesearch->ops->setup)(linesearch);CHKERRQ(ierr);
    }
    linesearch->lambda = linesearch->damping;
    linesearch->setupcalled = PETSC_TRUE;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchReset"

/*@
   PetscLineSearchReset - Tears down the structures required for application

   Collective on PetscLineSearch

   Input Parameters:
.  linesearch - The LineSearch instance.

   Level: Intermediate

   .keywords: PetscLineSearch, Create

   .seealso: PetscLineSearchSetUp()
@*/

PetscErrorCode PetscLineSearchReset(PetscLineSearch linesearch) {
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if (linesearch->ops->reset) {
    (*linesearch->ops->reset)(linesearch);
  }
  ierr = VecDestroy(&linesearch->vec_sol_new);CHKERRQ(ierr);
  ierr = VecDestroy(&linesearch->vec_func_new);CHKERRQ(ierr);

  ierr = VecDestroyVecs(linesearch->nwork, &linesearch->work);CHKERRQ(ierr);
  linesearch->nwork = 0;
  linesearch->setupcalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetPreCheck"
/*@C
   PetscLineSearchSetPreCheck - Sets a pre-check function for the line search routine.

   Logically Collective on PetscLineSearch

   Input Parameters:
+  linesearch - the PetscLineSearch context
.  func       - [optional] function evaluation routine
-  ctx        - [optional] user-defined context for private data for the
                function evaluation routine (may be PETSC_NULL)

   Calling sequence of func:
$    func (PetscLineSearch snes,Vec x,Vec y, PetscBool *changed);

+  x - solution vector
.  y - search direction vector
-  changed - flag to indicate the precheck changed something.

   Level: intermediate

.keywords: set, linesearch, pre-check

.seealso: PetscLineSearchSetPostCheck()
@*/
PetscErrorCode  PetscLineSearchSetPreCheck(PetscLineSearch linesearch, PetscLineSearchPreCheckFunc func,void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (func) linesearch->ops->precheckstep = func;
  if (ctx) linesearch->precheckctx = ctx;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetPreCheck"
/*@C
   PetscLineSearchSetPreCheck - Sets a pre-check function for the line search routine.

   Input Parameters:
.  linesearch - the PetscLineSearch context

   Output Parameters:
+  func       - [optional] function evaluation routine
-  ctx        - [optional] user-defined context for private data for the
                function evaluation routine (may be PETSC_NULL)

   Level: intermediate

.keywords: get, linesearch, pre-check

.seealso: PetscLineSearchGetPostCheck(), PetscLineSearchSetPreCheck()
@*/
PetscErrorCode  PetscLineSearchGetPreCheck(PetscLineSearch linesearch, PetscLineSearchPreCheckFunc *func,void **ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (func) *func = linesearch->ops->precheckstep;
  if (ctx) *ctx = linesearch->precheckctx;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetPostCheck"
/*@C
   PetscLineSearchSetPostCheck - Sets a post-check function for the line search routine.

   Logically Collective on PetscLineSearch

   Input Parameters:
+  linesearch - the PetscLineSearch context
.  func       - [optional] function evaluation routine
-  ctx        - [optional] user-defined context for private data for the
                function evaluation routine (may be PETSC_NULL)

   Calling sequence of func:
$    func (PetscLineSearch linesearch,Vec x, Vec w, Vec y, PetscBool *changed_w, *changed_y);

+  x - old solution vector
.  y - search direction vector
.  w - new solution vector
.  changed_y - indicates that the line search changed y.
.  changed_w - indicates that the line search changed w.

   Level: intermediate

.keywords: set, linesearch, post-check

.seealso: PetscLineSearchSetPreCheck()
@*/
PetscErrorCode  PetscLineSearchSetPostCheck(PetscLineSearch linesearch, PetscLineSearchPostCheckFunc func,void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (func) linesearch->ops->postcheckstep = func;
  if (ctx) linesearch->postcheckctx = ctx;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetPostCheck"
/*@C
   PetscLineSearchGetPostCheck - Gets the post-check function for the line search routine.

   Input Parameters:
.  linesearch - the PetscLineSearch context

   Output Parameters:
+  func       - [optional] function evaluation routine
-  ctx        - [optional] user-defined context for private data for the
                function evaluation routine (may be PETSC_NULL)

   Level: intermediate

.keywords: get, linesearch, post-check

.seealso: PetscLineSearchGetPreCheck(), PetscLineSearchSetPostCheck()
@*/
PetscErrorCode  PetscLineSearchGetPostCheck(PetscLineSearch linesearch, PetscLineSearchPostCheckFunc *func,void **ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (func) *func = linesearch->ops->postcheckstep;
  if (ctx) *ctx = linesearch->postcheckctx;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchPreCheck"
/*@
   PetscLineSearchPreCheck - Prepares the line search for being applied.

   Collective on PetscLineSearch

   Input Parameters:
.  linesearch - The linesearch instance.

   Output Parameters:
.  changed - Indicator if the pre-check has changed anything.

   Level: Beginner

   .keywords: PetscLineSearch, Create

   .seealso: PetscLineSearchPostCheck()
@*/
PetscErrorCode PetscLineSearchPreCheck(PetscLineSearch linesearch, PetscBool * changed)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  *changed = PETSC_FALSE;
  if (linesearch->ops->precheckstep) {
    ierr = (*linesearch->ops->precheckstep)(linesearch, linesearch->vec_sol, linesearch->vec_update, changed);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchPostCheck"
/*@
   PetscLineSearchPostCheck - Prepares the line search for being applied.

   Collective on PetscLineSearch

   Input Parameters:
.  linesearch - The linesearch instance.

   Output Parameters:
+  changed_Y - Indicator if the solution has been changed.
-  changed_W - Indicator if the direction has been changed.

   Level: Intermediate

   .keywords: PetscLineSearch, Create

   .seealso: PetscLineSearchPreCheck()
@*/
PetscErrorCode PetscLineSearchPostCheck(PetscLineSearch linesearch, PetscBool * changed_Y, PetscBool * changed_W)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  *changed_Y = PETSC_FALSE;
  *changed_W = PETSC_FALSE;
  if (linesearch->ops->postcheckstep) {
    ierr = (*linesearch->ops->postcheckstep)(linesearch, linesearch->vec_sol, linesearch->vec_update, linesearch->vec_sol_new, changed_Y, changed_W);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchPreCheckPicard"
/*@C
   SNESLineSearchPreCheckPicard - Implements a correction that is sometimes useful to improve the convergence rate of Picard iteration

   Logically Collective

   Input Arguments:
+  linesearch - linesearch context
.  X - base state for this step
.  Y - initial correction

   Output Arguments:
+  Y - correction, possibly modified
-  changed - flag indicating that Y was modified

   Options Database Key:
+  -snes_ls_precheck_picard - activate this routine
-  -snes_ls_precheck_picard_angle - angle

   Level: advanced

   Notes:
   This function should be passed to SNESLineSearchSetPreCheck()

   The justification for this method involves the linear convergence of a Picard iteration
   so the Picard linearization should be provided in place of the "Jacobian". This correction
   is generally not useful when using a Newton linearization.

   Reference:
   Hindmarsh and Payne (1996) Time step limits for stable solutions of the ice sheet equation, Annals of Glaciology.

.seealso: SNESLineSearchSetPreCheck()
@*/
PetscErrorCode PetscLineSearchPreCheckPicard(PetscLineSearch linesearch,Vec X,Vec Y,PetscBool *changed)
{
  PetscErrorCode ierr;
  PetscReal      angle = *(PetscReal*)linesearch->precheckctx;
  Vec            Ylast;
  PetscScalar    dot;
  PetscInt       iter;
  PetscReal      ynorm,ylastnorm,theta,angle_radians;
  SNES           snes;

  PetscFunctionBegin;
  ierr = PetscLineSearchGetSNES(linesearch, &snes);CHKERRQ(ierr);
  ierr = PetscObjectQuery((PetscObject)snes,"SNESLineSearchPreCheckPicard_Ylast",(PetscObject*)&Ylast);CHKERRQ(ierr);
  if (!Ylast) {
    ierr = VecDuplicate(Y,&Ylast);CHKERRQ(ierr);
    ierr = PetscObjectCompose((PetscObject)snes,"SNESLineSearchPreCheckPicard_Ylast",(PetscObject)Ylast);CHKERRQ(ierr);
    ierr = PetscObjectDereference((PetscObject)Ylast);CHKERRQ(ierr);
  }
  ierr = SNESGetIterationNumber(snes,&iter);CHKERRQ(ierr);
  if (iter < 2) {
    ierr = VecCopy(Y,Ylast);CHKERRQ(ierr);
    *changed = PETSC_FALSE;
    PetscFunctionReturn(0);
  }

  ierr = VecDot(Y,Ylast,&dot);CHKERRQ(ierr);
  ierr = VecNorm(Y,NORM_2,&ynorm);CHKERRQ(ierr);
  ierr = VecNorm(Ylast,NORM_2,&ylastnorm);CHKERRQ(ierr);
  /* Compute the angle between the vectors Y and Ylast, clip to keep inside the domain of acos() */
  theta = acos((double)PetscClipInterval(PetscAbsScalar(dot) / (ynorm * ylastnorm),-1.0,1.0));
  angle_radians = angle * PETSC_PI / 180.;
  if (PetscAbsReal(theta) < angle_radians || PetscAbsReal(theta - PETSC_PI) < angle_radians) {
    /* Modify the step Y */
    PetscReal alpha,ydiffnorm;
    ierr = VecAXPY(Ylast,-1.0,Y);CHKERRQ(ierr);
    ierr = VecNorm(Ylast,NORM_2,&ydiffnorm);CHKERRQ(ierr);
    alpha = ylastnorm / ydiffnorm;
    ierr = VecCopy(Y,Ylast);CHKERRQ(ierr);
    ierr = VecScale(Y,alpha);CHKERRQ(ierr);
    ierr = PetscInfo3(snes,"Angle %G degrees less than threshold %G, corrected step by alpha=%G\n",theta*180./PETSC_PI,angle,alpha);CHKERRQ(ierr);
  } else {
    ierr = PetscInfo2(snes,"Angle %G degrees exceeds threshold %G, no correction applied\n",theta*180./PETSC_PI,angle);CHKERRQ(ierr);
    ierr = VecCopy(Y,Ylast);CHKERRQ(ierr);
    *changed = PETSC_FALSE;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchApply"
/*@
   PetscLineSearchApply - Computes the line-search update

   Collective on PetscLineSearch

   Input Parameters:
+  linesearch - The linesearch instance.
.  X - The current solution.
.  F - The current function.
.  fnorm - The current norm.
.  Y - The search direction.

   Output Parameters:
+  X - The new solution.
.  F - The new function.
-  fnorm - The new function norm.

   Level: Intermediate

   .keywords: PetscLineSearch, Create

   .seealso: PetscLineSearchCreate(), PetscLineSearchPreCheck(), PetscLineSearchPostCheck()
@*/
PetscErrorCode PetscLineSearchApply(PetscLineSearch linesearch, Vec X, Vec F, PetscReal * fnorm, Vec Y) {
  PetscErrorCode ierr;
  PetscFunctionBegin;

  /* check the pointers */
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscValidHeaderSpecific(F,VEC_CLASSID,3);
  PetscValidHeaderSpecific(Y,VEC_CLASSID,4);

  linesearch->success = PETSC_TRUE;

  linesearch->vec_sol = X;
  linesearch->vec_update = Y;
  linesearch->vec_func = F;

  ierr = PetscLineSearchSetUp(linesearch);CHKERRQ(ierr);

  if (!linesearch->keeplambda)
    linesearch->lambda = linesearch->damping; /* set the initial guess to lambda */

  if (fnorm) {
    linesearch->fnorm = *fnorm;
  } else {
    ierr = VecNorm(F, NORM_2, &linesearch->fnorm);CHKERRQ(ierr);
  }

  ierr = PetscLogEventBegin(PetscLineSearch_Apply,linesearch,X,F,Y);CHKERRQ(ierr);

  ierr = (*linesearch->ops->apply)(linesearch);CHKERRQ(ierr);

  ierr = PetscLogEventEnd(PetscLineSearch_Apply,linesearch,X,F,Y);CHKERRQ(ierr);

  if (fnorm)
    *fnorm = linesearch->fnorm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchDestroy"
/*@
   PetscLineSearchDestroy - Destroys the line search instance.

   Collective on PetscLineSearch

   Input Parameters:
.  linesearch - The linesearch instance.

   Level: Intermediate

   .keywords: PetscLineSearch, Create

   .seealso: PetscLineSearchCreate(), PetscLineSearchReset()
@*/
PetscErrorCode PetscLineSearchDestroy(PetscLineSearch * linesearch) {
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if (!*linesearch) PetscFunctionReturn(0);
  PetscValidHeaderSpecific((*linesearch),PETSCLINESEARCH_CLASSID,1);
  if (--((PetscObject)(*linesearch))->refct > 0) {*linesearch = 0; PetscFunctionReturn(0);}
  ierr = PetscObjectDepublish((*linesearch));CHKERRQ(ierr);
  ierr = PetscLineSearchReset(*linesearch);
  if ((*linesearch)->ops->destroy) {
    (*linesearch)->ops->destroy(*linesearch);
  }
  ierr = PetscViewerDestroy(&(*linesearch)->monitor);CHKERRQ(ierr);
  ierr = PetscHeaderDestroy(linesearch);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetMonitor"
/*@
   PetscLineSearchSetMonitor - Turns on/off printing useful things about the line search.

   Input Parameters:
+  snes - nonlinear context obtained from SNESCreate()
-  flg - PETSC_TRUE to monitor the line search

   Logically Collective on SNES

   Options Database:
.   -linesearch_monitor - enables the monitor.

   Level: intermediate


.seealso: PetscLineSearchGetMonitor()
@*/
PetscErrorCode  PetscLineSearchSetMonitor(PetscLineSearch linesearch, PetscBool flg)
{

  PetscErrorCode ierr;
  PetscFunctionBegin;
  if (flg && !linesearch->monitor) {
    ierr = PetscViewerASCIIOpen(((PetscObject)linesearch)->comm,"stdout",&linesearch->monitor);CHKERRQ(ierr);
  } else if (!flg && linesearch->monitor) {
    ierr = PetscViewerDestroy(&linesearch->monitor);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetMonitor"
/*@
   PetscLineSearchGetMonitor - Gets the monitor instance for the line search

   Input Parameters:
.  linesearch - linesearch context.

   Input Parameters:
.  monitor - monitor context.

   Logically Collective on SNES


   Options Database Keys:
.   -linesearch_monitor - enables the monitor.

   Level: intermediate


.seealso: PetscLineSearchSetMonitor()
@*/
PetscErrorCode  PetscLineSearchGetMonitor(PetscLineSearch linesearch, PetscViewer *monitor)
{

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (monitor) {
    PetscValidPointer(monitor, 2);
    *monitor = linesearch->monitor;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetFromOptions"
/*@
   PetscLineSearchSetFromOptions - Sets options for the line search

   Input Parameters:
.  linesearch - linesearch context.

   Options Database Keys:
+ -linesearch_type - The Line search method
. -linesearch_monitor - Print progress of line searches
. -linesearch_damping - The linesearch damping parameter.
. -linesearch_norms   - Turn on/off the linesearch norms
. -linesearch_keeplambda - Keep the previous search length as the initial guess.
- -linesearch_max_it - The number of iterations for iterative line searches.

   Logically Collective on PetscLineSearch

   Level: intermediate


.seealso: PetscLineSearchCreate()
@*/
PetscErrorCode PetscLineSearchSetFromOptions(PetscLineSearch linesearch) {
  PetscErrorCode ierr;
  const char     *deft = PETSCLINESEARCHBASIC;
  char           type[256];
  PetscBool      flg, set;
  PetscFunctionBegin;
  if (!PetscLineSearchRegisterAllCalled) {ierr = PetscLineSearchRegisterAll(PETSC_NULL);CHKERRQ(ierr);}

  ierr = PetscObjectOptionsBegin((PetscObject)linesearch);CHKERRQ(ierr);
  if (((PetscObject)linesearch)->type_name) {
    deft = ((PetscObject)linesearch)->type_name;
  }
  ierr = PetscOptionsList("-linesearch_type","Line-search method","PetscLineSearchSetType",PetscLineSearchList,deft,type,256,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscLineSearchSetType(linesearch,type);CHKERRQ(ierr);
  } else if (!((PetscObject)linesearch)->type_name) {
    ierr = PetscLineSearchSetType(linesearch,deft);CHKERRQ(ierr);
  }
  if (linesearch->ops->setfromoptions) {
    (*linesearch->ops->setfromoptions)(linesearch);CHKERRQ(ierr);
  }

  ierr = PetscOptionsBool("-linesearch_monitor","Print progress of line searches","SNESPetscLineSearchSetMonitor",
                          linesearch->monitor ? PETSC_TRUE : PETSC_FALSE,&flg,&set);CHKERRQ(ierr);
  if (set) {ierr = PetscLineSearchSetMonitor(linesearch,flg);CHKERRQ(ierr);}

  ierr = PetscOptionsReal("-linesearch_damping","Line search damping and initial step guess","PetscLineSearchSetDamping",linesearch->damping,&linesearch->damping,0);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-linesearch_rtol","Tolerance for iterative line search","PetscLineSearchSetRTolerance",linesearch->rtol,&linesearch->rtol,0);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-linesearch_norms","Compute final norms in line search","PetscLineSearchSetComputeNorms",linesearch->norms,&linesearch->norms,0);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-linesearch_keeplambda","Use previous lambda as damping","PetscLineSearchSetKeepLambda",linesearch->keeplambda,&linesearch->keeplambda,0);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-linesearch_max_it","Maximum iterations for iterative line searches","",linesearch->max_its,&linesearch->max_its,0);CHKERRQ(ierr);
  ierr = PetscObjectProcessOptionsHandlers((PetscObject)linesearch);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchView"
/*@
   PetscLineSearchView - Views useful information for the line search.

   Input Parameters:
.  linesearch - linesearch context.

   Logically Collective on PetscLineSearch

   Level: intermediate


.seealso: PetscLineSearchCreate()
@*/
PetscErrorCode PetscLineSearchView(PetscLineSearch linesearch) {
  PetscFunctionBegin;

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetType"
/*@C
   PetscLineSearchSetType - Sets the linesearch type

   Input Parameters:
+  linesearch - linesearch context.
-  type - The type of line search to be used

   Logically Collective on PetscLineSearch

   Level: intermediate


.seealso: PetscLineSearchCreate()
@*/
PetscErrorCode PetscLineSearchSetType(PetscLineSearch linesearch, const PetscLineSearchType type)
{

  PetscErrorCode ierr,(*r)(PetscLineSearch);
  PetscBool      match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidCharPointer(type,2);

  ierr = PetscTypeCompare((PetscObject)linesearch,type,&match);CHKERRQ(ierr);
  if (match) PetscFunctionReturn(0);

  ierr =  PetscFListFind(PetscLineSearchList,((PetscObject)linesearch)->comm,type,PETSC_TRUE,(void (**)(void)) &r);CHKERRQ(ierr);
  if (!r) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_UNKNOWN_TYPE,"Unable to find requested Line Search type %s",type);
  /* Destroy the previous private linesearch context */
  if (linesearch->ops->destroy) {
    ierr = (*(linesearch)->ops->destroy)(linesearch);CHKERRQ(ierr);
    linesearch->ops->destroy = PETSC_NULL;
  }
  /* Reinitialize function pointers in PetscLineSearchOps structure */
  linesearch->ops->apply          = 0;
  linesearch->ops->view           = 0;
  linesearch->ops->setfromoptions = 0;
  linesearch->ops->destroy        = 0;

  ierr = PetscObjectChangeTypeName((PetscObject)linesearch,type);CHKERRQ(ierr);
  ierr = (*r)(linesearch);CHKERRQ(ierr);
#if defined(PETSC_HAVE_AMS)
  if (PetscAMSPublishAll) {
    ierr = PetscObjectAMSPublish((PetscObject)linesearch);CHKERRQ(ierr);
  }
#endif
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetSNES"
/*@
   PetscLineSearchSetSNES - Sets the SNES for the linesearch for function evaluation

   Input Parameters:
+  linesearch - linesearch context.
-  snes - The snes instance

   Level: intermediate


.seealso: PetscLineSearchGetSNES(), PetscLineSearchSetVecs()
@*/
PetscErrorCode  PetscLineSearchSetSNES(PetscLineSearch linesearch, SNES snes){
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidHeaderSpecific(snes,SNES_CLASSID,2);
  linesearch->snes = snes;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetSNES"
/*@
   PetscLineSearchGetSNES - Gets the SNES for the linesearch for function evaluation

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
.  snes - The snes instance

   Level: intermediate

.seealso: PetscLineSearchGetSNES(), PetscLineSearchSetVecs()
@*/
PetscErrorCode  PetscLineSearchGetSNES(PetscLineSearch linesearch, SNES *snes){
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidPointer(snes, 2);
  *snes = linesearch->snes;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetLambda"
/*@
   PetscLineSearchGetLambda - Gets the last linesearch steplength discovered.

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
.  lambda - The last steplength.

   Level: intermediate

.seealso: PetscLineSearchGetSNES(), PetscLineSearchSetVecs()
@*/
PetscErrorCode  PetscLineSearchGetLambda(PetscLineSearch linesearch,PetscReal *lambda)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidPointer(lambda, 2);
  *lambda = linesearch->lambda;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetLambda"
/*@
   PetscLineSearchSetLambda - Sets the linesearch steplength.

   Input Parameters:
+  linesearch - linesearch context.
-  lambda - The last steplength.

   Level: intermediate

.seealso: PetscLineSearchGetLambda()
@*/
PetscErrorCode  PetscLineSearchSetLambda(PetscLineSearch linesearch, PetscReal lambda)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  linesearch->lambda = lambda;
  PetscFunctionReturn(0);
}

#undef  __FUNCT__
#define __FUNCT__ "PetscLineSearchGetTolerances"
/*@
   PetscLineSearchGetTolerances - Gets the tolerances for the method

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
+  steptol - The minimum steplength
.  rtol    - The relative tolerance for iterative line searches
.  atol    - The absolute tolerance for iterative line searches
.  ltol    - The change in lambda tolerance for iterative line searches
-  max_it  - The maximum number of iterations of the line search


   Level: advanced

.seealso: PetscLineSearchSetTolerances()
@*/
PetscErrorCode  PetscLineSearchGetTolerances(PetscLineSearch linesearch,PetscReal *steptol,PetscReal *maxstep, PetscReal *rtol, PetscReal *atol, PetscReal *ltol, PetscInt *max_its)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (steptol) {
    PetscValidPointer(steptol, 2);
    *steptol = linesearch->steptol;
  }
  if (maxstep) {
    PetscValidPointer(maxstep, 3);
    *maxstep = linesearch->maxstep;
  }
  if (rtol) {
    PetscValidPointer(rtol, 4);
    *rtol = linesearch->rtol;
  }
  if (atol) {
    PetscValidPointer(atol, 5);
    *atol = linesearch->atol;
  }
  if (ltol) {
    PetscValidPointer(ltol, 6);
    *ltol = linesearch->ltol;
  }
  if (max_its) {
    PetscValidPointer(max_its, 7);
    *max_its = linesearch->max_its;
  }
  PetscFunctionReturn(0);
}

#undef  __FUNCT__
#define __FUNCT__ "PetscLineSearchSetTolerances"
/*@
   PetscLineSearchSetTolerances - Sets the tolerances for the method

   Input Parameters:
+  linesearch - linesearch context.
.  steptol - The minimum steplength
.  rtol    - The relative tolerance for iterative line searches
.  atol    - The absolute tolerance for iterative line searches
.  ltol    - The change in lambda tolerance for iterative line searches
-  max_it  - The maximum number of iterations of the line search


   Level: advanced

.seealso: PetscLineSearchGetTolerances()
@*/
PetscErrorCode  PetscLineSearchSetTolerances(PetscLineSearch linesearch,PetscReal steptol,PetscReal maxstep, PetscReal rtol, PetscReal atol, PetscReal ltol, PetscInt max_its)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  linesearch->steptol = steptol;
  linesearch->maxstep = maxstep;
  linesearch->rtol = rtol;
  linesearch->atol = atol;
  linesearch->ltol = ltol;
  linesearch->max_its = max_its;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetDamping"
/*@
   PetscLineSearchGetDamping - Gets the line search damping parameter.

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
.  damping - The damping parameter.

   Level: intermediate

.seealso: PetscLineSearchGetStepTolerance()
@*/

PetscErrorCode  PetscLineSearchGetDamping(PetscLineSearch linesearch,PetscReal *damping)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidPointer(damping, 2);
  *damping = linesearch->damping;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetDamping"
/*@
   PetscLineSearchSetDamping - Sets the line search damping paramter.

   Input Parameters:
.  linesearch - linesearch context.
.  damping - The damping parameter.

   Level: intermediate

.seealso: PetscLineSearchGetDamping()
@*/
PetscErrorCode  PetscLineSearchSetDamping(PetscLineSearch linesearch,PetscReal damping)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  linesearch->damping = damping;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetNorms"
/*@
   PetscLineSearchGetNorms - Gets the norms for for X, Y, and F.

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
+  xnorm - The norm of the current solution
.  fnorm - The norm of the current function
-  ynorm - The norm of the current update

   Level: intermediate

.seealso: PetscLineSearchSetNorms() PetscLineSearchGetVecs()
@*/
PetscErrorCode  PetscLineSearchGetNorms(PetscLineSearch linesearch, PetscReal * xnorm, PetscReal * fnorm, PetscReal * ynorm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (xnorm) {
    *xnorm = linesearch->xnorm;
  }
  if (fnorm) {
    *fnorm = linesearch->fnorm;
  }
  if (ynorm) {
    *ynorm = linesearch->ynorm;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetNorms"
/*@
   PetscLineSearchSetNorms - Gets the computed norms for for X, Y, and F.

   Input Parameters:
+  linesearch - linesearch context.
.  xnorm - The norm of the current solution
.  fnorm - The norm of the current function
-  ynorm - The norm of the current update

   Level: intermediate

.seealso: PetscLineSearchGetNorms(), PetscLineSearchSetVecs()
@*/
PetscErrorCode  PetscLineSearchSetNorms(PetscLineSearch linesearch, PetscReal xnorm, PetscReal fnorm, PetscReal ynorm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  linesearch->xnorm = xnorm;
  linesearch->fnorm = fnorm;
  linesearch->ynorm = ynorm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchComputeNorms"
/*@
   PetscLineSearchComputeNorms - Computes the norms of X, F, and Y.

   Input Parameters:
.  linesearch - linesearch context.

   Options Database Keys:
.   -linesearch_norms - turn norm computation on or off.

   Level: intermediate

.seealso: PetscLineSearchGetNorms, PetscLineSearchSetNorms()
@*/
PetscErrorCode PetscLineSearchComputeNorms(PetscLineSearch linesearch)
{
  PetscErrorCode ierr;
  SNES snes;
  PetscFunctionBegin;
  if (linesearch->norms) {
    if (linesearch->ops->vinorm) {
      ierr = PetscLineSearchGetSNES(linesearch, &snes);CHKERRQ(ierr);
      ierr = VecNorm(linesearch->vec_sol, NORM_2, &linesearch->xnorm);CHKERRQ(ierr);
      ierr = VecNorm(linesearch->vec_update, NORM_2, &linesearch->ynorm);CHKERRQ(ierr);
      ierr = (*linesearch->ops->vinorm)(snes, linesearch->vec_func, linesearch->vec_sol, &linesearch->fnorm);CHKERRQ(ierr);
    } else {
      ierr = VecNormBegin(linesearch->vec_func,   NORM_2, &linesearch->fnorm);CHKERRQ(ierr);
      ierr = VecNormBegin(linesearch->vec_sol,    NORM_2, &linesearch->xnorm);CHKERRQ(ierr);
      ierr = VecNormBegin(linesearch->vec_update, NORM_2, &linesearch->ynorm);CHKERRQ(ierr);
      ierr = VecNormEnd(linesearch->vec_func,     NORM_2, &linesearch->fnorm);CHKERRQ(ierr);
      ierr = VecNormEnd(linesearch->vec_sol,      NORM_2, &linesearch->xnorm);CHKERRQ(ierr);
      ierr = VecNormEnd(linesearch->vec_update,   NORM_2, &linesearch->ynorm);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetVecs"
/*@
   PetscLineSearchGetVecs - Gets the vectors from the PetscLineSearch context

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
+  X - The old solution
.  F - The old function
.  Y - The search direction
.  W - The new solution
-  G - The new function

   Level: intermediate

.seealso: PetscLineSearchGetNorms(), PetscLineSearchSetVecs()
@*/
PetscErrorCode PetscLineSearchGetVecs(PetscLineSearch linesearch,Vec *X,Vec *F, Vec *Y,Vec *W,Vec *G) {
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (X) {
    PetscValidPointer(X, 2);
    *X = linesearch->vec_sol;
  }
  if (F) {
    PetscValidPointer(F, 3);
    *F = linesearch->vec_func;
  }
  if (Y) {
    PetscValidPointer(Y, 4);
    *Y = linesearch->vec_update;
  }
  if (W) {
    PetscValidPointer(W, 5);
    *W = linesearch->vec_sol_new;
  }
  if (G) {
    PetscValidPointer(G, 6);
    *G = linesearch->vec_func_new;
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetVecs"
/*@
   PetscLineSearchSetVecs - Sets the vectors on the PetscLineSearch context

   Input Parameters:
+  linesearch - linesearch context.
.  X - The old solution
.  F - The old function
.  Y - The search direction
.  W - The new solution
-  G - The new function

   Level: intermediate

.seealso: PetscLineSearchSetNorms(), PetscLineSearchGetVecs()
@*/
PetscErrorCode PetscLineSearchSetVecs(PetscLineSearch linesearch,Vec X,Vec F,Vec Y,Vec W, Vec G) {
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (X) {
    PetscValidHeaderSpecific(X,VEC_CLASSID,2);
    linesearch->vec_sol = X;
  }
  if (F) {
    PetscValidHeaderSpecific(F,VEC_CLASSID,3);
    linesearch->vec_func = F;
  }
  if (Y) {
    PetscValidHeaderSpecific(Y,VEC_CLASSID,4);
    linesearch->vec_update = Y;
  }
  if (W) {
    PetscValidHeaderSpecific(W,VEC_CLASSID,5);
    linesearch->vec_sol_new = W;
  }
  if (G) {
    PetscValidHeaderSpecific(G,VEC_CLASSID,6);
    linesearch->vec_func_new = G;
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchAppendOptionsPrefix"
/*@C
   PetscLineSearchAppendOptionsPrefix - Appends to the prefix used for searching for all
   SNES options in the database.

   Logically Collective on SNES

   Input Parameters:
+  snes - the SNES context
-  prefix - the prefix to prepend to all option names

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

   Level: advanced

.keywords: PetscLineSearch, append, options, prefix, database

.seealso: SNESGetOptionsPrefix()
@*/
PetscErrorCode  PetscLineSearchAppendOptionsPrefix(PetscLineSearch linesearch,const char prefix[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  ierr = PetscObjectAppendOptionsPrefix((PetscObject)linesearch,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetOptionsPrefix"
/*@C
   PetscLineSearchGetOptionsPrefix - Sets the prefix used for searching for all
   PetscLineSearch options in the database.

   Not Collective

   Input Parameter:
.  snes - the SNES context

   Output Parameter:
.  prefix - pointer to the prefix string used

   Notes: On the fortran side, the user should pass in a string 'prefix' of
   sufficient length to hold the prefix.

   Level: advanced

.keywords: PetscLineSearch, get, options, prefix, database

.seealso: SNESAppendOptionsPrefix()
@*/
PetscErrorCode  PetscLineSearchGetOptionsPrefix(PetscLineSearch linesearch,const char *prefix[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  ierr = PetscObjectGetOptionsPrefix((PetscObject)linesearch,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetWork"
/*@
   PetscLineSearchGetWork - Gets work vectors for the line search.

   Input Parameter:
+  linesearch - the PetscLineSearch context
-  nwork - the number of work vectors

   Level: developer

.keywords: PetscLineSearch, work, vector

.seealso: SNESDefaultGetWork()
@*/
PetscErrorCode  PetscLineSearchGetWork(PetscLineSearch linesearch, PetscInt nwork)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  if (linesearch->vec_sol) {
    ierr = VecDuplicateVecs(linesearch->vec_sol, nwork, &linesearch->work);CHKERRQ(ierr);
  } else {
    SETERRQ(((PetscObject)linesearch)->comm, PETSC_ERR_USER, "Cannot get linesearch work-vectors without setting a solution vec!");
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetSuccess"
/*@
   PetscLineSearchGetSuccess - Gets the success/failure status of the last line search application

   Input Parameters:
.  linesearch - linesearch context.

   Output Parameters:
.  success - The success or failure status.

   Level: intermediate

.seealso: PetscLineSearchSetSuccess()
@*/
PetscErrorCode  PetscLineSearchGetSuccess(PetscLineSearch linesearch, PetscBool *success)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscValidPointer(success, 2);
  if (success) {
    *success = linesearch->success;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetSuccess"
/*@
   PetscLineSearchSetSuccess - Sets the success/failure status of the last line search application

   Input Parameters:
+  linesearch - linesearch context.
-  success - The success or failure status.

   Level: intermediate

.seealso: PetscLineSearchGetSuccess()
@*/
PetscErrorCode  PetscLineSearchSetSuccess(PetscLineSearch linesearch, PetscBool success)
{
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  PetscFunctionBegin;
  linesearch->success = success;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchSetVIFunctions"
/*@C
   PetscLineSearchSetVIFunctions - Sets VI-specific functions for line search computation.

   Input Parameters:
+  snes - nonlinear context obtained from SNESCreate()
.  projectfunc - function for projecting the function to the bounds
-  normfunc - function for computing the norm of an active set

   Logically Collective on SNES

   Calling sequence of projectfunc:
.vb
   projectfunc (SNES snes, Vec X)
.ve

    Input parameters for projectfunc:
+   snes - nonlinear context
-   X - current solution

    Output parameters for func:
.   X - Projected solution

   Calling sequence of normfunc:
.vb
   projectfunc (SNES snes, Vec X, Vec F, PetscScalar * fnorm)
.ve

    Input parameters for projectfunc:
+   snes - nonlinear context
.   X - current solution
-   F - current residual

    Output parameters for func:
.   fnorm - VI-specific norm of the function


    Level: developer

.keywords: SNES, line search, VI, nonlinear, set, line search

.seealso: PetscLineSearchGetVIFunctions(), PetscLineSearchSetPostCheck(), PetscLineSearchSetPreCheck()
@*/
extern PetscErrorCode PetscLineSearchSetVIFunctions(PetscLineSearch linesearch, PetscLineSearchVIProjectFunc projectfunc, PetscLineSearchVINormFunc normfunc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch,PETSCLINESEARCH_CLASSID,1);
  if (projectfunc) linesearch->ops->viproject = projectfunc;
  if (normfunc) linesearch->ops->vinorm = normfunc;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchGetVIFunctions"
/*@C
   PetscLineSearchGetVIFunctions - Sets VI-specific functions for line search computation.

   Input Parameters:
.  snes - nonlinear context obtained from SNESCreate()

   Output Parameters:
+  projectfunc - function for projecting the function to the bounds
-  normfunc - function for computing the norm of an active set

   Logically Collective on SNES

    Level: developer

.keywords: SNES, line search, VI, nonlinear, get, line search

.seealso: PetscLineSearchSetVIFunctions(), PetscLineSearchGetPostCheck(), PetscLineSearchGetPreCheck()
@*/
extern PetscErrorCode PetscLineSearchGetVIFunctions(PetscLineSearch linesearch, PetscLineSearchVIProjectFunc *projectfunc, PetscLineSearchVINormFunc *normfunc)
{
  PetscFunctionBegin;
  if (projectfunc) *projectfunc = linesearch->ops->viproject;
  if (normfunc) *normfunc = linesearch->ops->vinorm;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscLineSearchRegister"
/*@C
  PetscLineSearchRegister - See PetscLineSearchRegisterDynamic()

  Level: advanced
@*/
PetscErrorCode  PetscLineSearchRegister(const char sname[],const char path[],const char name[],PetscErrorCode (*function)(PetscLineSearch))
{
  char           fullname[PETSC_MAX_PATH_LEN];
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFListConcat(path,name,fullname);CHKERRQ(ierr);
  ierr = PetscFListAdd(&PetscLineSearchList,sname,fullname,(void (*)(void))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
