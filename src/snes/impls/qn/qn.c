#include <petsc-private/snesimpl.h>

#define H(i,j)  qn->dXdFmat[i*qn->m + j]

typedef enum {SNES_QN_SEQUENTIAL, SNES_QN_COMPOSED} SNESQNCompositionType;
typedef enum {SNES_QN_SHANNOSCALE, SNES_QN_LSSCALE, SNES_QN_JACOBIANSCALE} SNESQNScalingType;

typedef struct {
  Vec          *dX;              /* The change in X */
  Vec          *dF;              /* The change in F */
  PetscInt     m;                /* The number of kept previous steps */
  PetscScalar  *alpha, *beta;
  PetscScalar  *dXtdF, *dFtdX, *YtdX;
  PetscBool    aggreduct;        /* Aggregated reduction implementation */
  PetscScalar  *dXdFmat;         /* A matrix of values for dX_i dot dF_j */
  PetscViewer  monitor;
  PetscReal    powell_gamma;     /* Powell angle restart condition */
  PetscReal    powell_downhill;  /* Powell descent restart condition */
  PetscReal    scaling;          /* scaling of H0 */
  PetscInt     n_restart;        /* the maximum iterations between restart */

  SNESQNCompositionType compositiontype; /* determine if the composition is done sequentially or as a composition */
  SNESQNScalingType scalingtype; /* determine if the composition is done sequentially or as a composition */

} SNES_QN;

#undef __FUNCT__
#define __FUNCT__ "SNESQNApplyJinv_Private"
PetscErrorCode SNESQNApplyJinv_Private(SNES snes, PetscInt it, Vec D, Vec Y) {

  PetscErrorCode ierr;

  SNES_QN *qn = (SNES_QN*)snes->data;

  Vec Yin = snes->work[3];

  Vec *dX = qn->dX;
  Vec *dF = qn->dF;

  PetscScalar *alpha    = qn->alpha;
  PetscScalar *beta     = qn->beta;
  PetscScalar *dXtdF    = qn->dXtdF;
  PetscScalar *YtdX     = qn->YtdX;

  /* ksp thing for jacobian scaling */
  KSPConvergedReason kspreason;
  MatStructure       flg = DIFFERENT_NONZERO_PATTERN;

  PetscInt k, i, j, g, lits;
  PetscInt m = qn->m;
  PetscScalar t;
  PetscInt l = m;

  Mat jac, jac_pre;

  PetscFunctionBegin;

  ierr = VecCopy(D, Y);CHKERRQ(ierr);

  if (it < m) l = it;

  if (qn->aggreduct) {
    ierr = VecMDot(Y, l, qn->dX, YtdX);CHKERRQ(ierr);
  }
  /* outward recursion starting at iteration k's update and working back */
  for (i = 0; i < l; i++) {
    k = (it - i - 1) % l;
    if (qn->aggreduct) {
      /* construct t = dX[k] dot Y as Y_0 dot dX[k] + sum(-alpha[j]dX[k]dF[j]) */
      t = YtdX[k];
      for (j = 0; j < i; j++) {
        g = (it - j - 1) % l;
        t += -alpha[g]*H(g, k);
      }
      alpha[k] = t / H(k, k);
    } else {
      ierr = VecDot(dX[k], Y, &t);CHKERRQ(ierr);
      alpha[k] = t / dXtdF[k];
    }
    if (qn->monitor) {
      ierr = PetscViewerASCIIAddTab(qn->monitor,((PetscObject)snes)->tablevel+2);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(qn->monitor, "it: %d k: %d alpha:        %14.12e\n", it, k, PetscRealPart(alpha[k]));CHKERRQ(ierr);
      ierr = PetscViewerASCIISubtractTab(qn->monitor,((PetscObject)snes)->tablevel+2);CHKERRQ(ierr);
    }
    ierr = VecAXPY(Y, -alpha[k], dF[k]);CHKERRQ(ierr);
  }

  if (qn->scalingtype == SNES_QN_JACOBIANSCALE) {
    ierr = SNESGetJacobian(snes, &jac, &jac_pre, PETSC_NULL, PETSC_NULL);CHKERRQ(ierr);
    ierr = KSPSetOperators(snes->ksp,jac,jac_pre,flg);CHKERRQ(ierr);
    ierr = SNES_KSPSolve(snes,snes->ksp,Y,Yin);CHKERRQ(ierr);
    ierr = KSPGetConvergedReason(snes->ksp,&kspreason);CHKERRQ(ierr);
    if (kspreason < 0) {
      if (++snes->numLinearSolveFailures >= snes->maxLinearSolveFailures) {
        ierr = PetscInfo2(snes,"iter=%D, number linear solve failures %D greater than current SNES allowed, stopping solve\n",snes->iter,snes->numLinearSolveFailures);CHKERRQ(ierr);
        snes->reason = SNES_DIVERGED_LINEAR_SOLVE;
        PetscFunctionReturn(0);
      }
    }
    ierr = KSPGetIterationNumber(snes->ksp,&lits);CHKERRQ(ierr);
    snes->linear_its += lits;
    ierr = VecCopy(Yin, Y);CHKERRQ(ierr);
  } else {
    ierr = VecScale(Y, qn->scaling);CHKERRQ(ierr);
  }
  if (qn->aggreduct) {
    ierr = VecMDot(Y, l, qn->dF, YtdX);CHKERRQ(ierr);
  }
  /* inward recursion starting at the first update and working forward */
  for (i = 0; i < l; i++) {
    k = (it + i - l) % l;
    if (qn->aggreduct) {
      t = YtdX[k];
      for (j = 0; j < i; j++) {
        g = (it + j - l) % l;
        t += (alpha[g] - beta[g])*H(k, g);
      }
      beta[k] = t / H(k, k);
    } else {
      ierr = VecDot(dF[k], Y, &t);CHKERRQ(ierr);
      beta[k] = t / dXtdF[k];
    }
    ierr = VecAXPY(Y, (alpha[k] - beta[k]), dX[k]);
    if (qn->monitor) {
      ierr = PetscViewerASCIIAddTab(qn->monitor,((PetscObject)snes)->tablevel+2);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(qn->monitor, "it: %d k: %d alpha - beta: %14.12e\n", it, k, PetscRealPart(alpha[k] - beta[k]));CHKERRQ(ierr);
      ierr = PetscViewerASCIISubtractTab(qn->monitor,((PetscObject)snes)->tablevel+2);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESSolve_QN"
static PetscErrorCode SNESSolve_QN(SNES snes)
{

  PetscErrorCode ierr;
  SNES_QN *qn = (SNES_QN*) snes->data;

  Vec X, Xold;
  Vec F, B;
  Vec Y, FPC, D, Dold;
  SNESConvergedReason reason;
  PetscInt i, i_r, k, l, j;

  PetscReal fnorm, xnorm, ynorm, gnorm;
  PetscInt m = qn->m;
  PetscBool lssucceed;

  Vec *dX = qn->dX;
  Vec *dF = qn->dF;
  PetscScalar *dXtdF = qn->dXtdF;
  PetscScalar *dFtdX = qn->dFtdX;
  PetscScalar DolddotD, DolddotDold, DdotD, YdotD, a;

  MatStructure       flg = DIFFERENT_NONZERO_PATTERN;

  /* basically just a regular newton's method except for the application of the jacobian */
  PetscFunctionBegin;

  X             = snes->vec_sol;        /* solution vector */
  F             = snes->vec_func;       /* residual vector */
  Y             = snes->vec_sol_update; /* search direction generated by J^-1D*/
  B             = snes->vec_rhs;
  Xold          = snes->work[0];

  /* directions generated by the preconditioned problem with F_pre = F or x - M(x, b) */
  D             = snes->work[1];
  Dold          = snes->work[2];

  snes->reason = SNES_CONVERGED_ITERATING;

  ierr = PetscObjectTakeAccess(snes);CHKERRQ(ierr);
  snes->iter = 0;
  snes->norm = 0.;
  ierr = PetscObjectGrantAccess(snes);CHKERRQ(ierr);
  if (!snes->vec_func_init_set){
    ierr = SNESComputeFunction(snes,X,F);CHKERRQ(ierr);
    if (snes->domainerror) {
      snes->reason = SNES_DIVERGED_FUNCTION_DOMAIN;
      PetscFunctionReturn(0);
    }
  } else {
    snes->vec_func_init_set = PETSC_FALSE;
  }

  if (!snes->norm_init_set) {
    ierr = VecNorm(F, NORM_2, &fnorm);CHKERRQ(ierr); /* fnorm <- ||F||  */
    if (PetscIsInfOrNanReal(fnorm)) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_FP,"Infinite or not-a-number generated in norm");
  } else {
    fnorm = snes->norm_init;
    snes->norm_init_set = PETSC_FALSE;
  }

  ierr = PetscObjectTakeAccess(snes);CHKERRQ(ierr);
  snes->norm = fnorm;
  ierr = PetscObjectGrantAccess(snes);CHKERRQ(ierr);
  SNESLogConvHistory(snes,fnorm,0);
  ierr = SNESMonitor(snes,0,fnorm);CHKERRQ(ierr);

  /* set parameter for default relative tolerance convergence test */
   snes->ttol = fnorm*snes->rtol;
  /* test convergence */
  ierr = (*snes->ops->converged)(snes,0,0.0,0.0,fnorm,&snes->reason,snes->cnvP);CHKERRQ(ierr);
  if (snes->reason) PetscFunctionReturn(0);

  /* composed solve -- either sequential or composed */
  if (snes->pc) {
    if (qn->compositiontype == SNES_QN_SEQUENTIAL) {
      ierr = SNESSolve(snes->pc, B, X);CHKERRQ(ierr);
      ierr = SNESGetConvergedReason(snes->pc,&reason);CHKERRQ(ierr);
      if (reason < 0 && (reason != SNES_DIVERGED_MAX_IT)) {
        snes->reason = SNES_DIVERGED_INNER;
        PetscFunctionReturn(0);
      }
      ierr = SNESGetFunction(snes->pc, &FPC, PETSC_NULL, PETSC_NULL);CHKERRQ(ierr);
      ierr = VecCopy(FPC, F);CHKERRQ(ierr);
      ierr = SNESGetFunctionNorm(snes->pc, &fnorm);CHKERRQ(ierr);
      ierr = VecCopy(F, Y);CHKERRQ(ierr);
    } else {
      ierr = VecCopy(X, Y);CHKERRQ(ierr);
      ierr = SNESSolve(snes->pc, B, Y);CHKERRQ(ierr);
      ierr = SNESGetConvergedReason(snes->pc,&reason);CHKERRQ(ierr);
      if (reason < 0 && (reason != SNES_DIVERGED_MAX_IT)) {
        snes->reason = SNES_DIVERGED_INNER;
        PetscFunctionReturn(0);
      }
      ierr = VecAYPX(Y,-1.0,X);CHKERRQ(ierr);
    }
  } else {
    ierr = VecCopy(F, Y);CHKERRQ(ierr);
  }
  ierr = VecCopy(Y, D);CHKERRQ(ierr);

  /* scale the initial update */
  if (qn->scalingtype == SNES_QN_JACOBIANSCALE) {
    ierr = SNESComputeJacobian(snes,X,&snes->jacobian,&snes->jacobian_pre,&flg);CHKERRQ(ierr);
  }

  for(i = 0, i_r = 0; i < snes->max_its; i++, i_r++) {
    ierr = SNESQNApplyJinv_Private(snes, i_r, D, Y);CHKERRQ(ierr);
    /* line search for lambda */
    ynorm = 1; gnorm = fnorm;
    ierr = VecCopy(D, Dold);CHKERRQ(ierr);
    ierr = VecCopy(X, Xold);CHKERRQ(ierr);
    ierr = SNESLineSearchApply(snes->linesearch, X, F, &fnorm, Y);CHKERRQ(ierr);
    if (snes->reason == SNES_DIVERGED_FUNCTION_COUNT) break;
    if (snes->domainerror) {
      snes->reason = SNES_DIVERGED_FUNCTION_DOMAIN;
      PetscFunctionReturn(0);
      }
    ierr = SNESLineSearchGetSuccess(snes->linesearch, &lssucceed);CHKERRQ(ierr);
    if (!lssucceed) {
      if (++snes->numFailures >= snes->maxFailures) {
        snes->reason = SNES_DIVERGED_LINE_SEARCH;
        break;
      }
    }
    ierr = SNESLineSearchGetNorms(snes->linesearch, &xnorm, &fnorm, &ynorm);CHKERRQ(ierr);
    if (qn->scalingtype == SNES_QN_LSSCALE) {
      ierr = SNESLineSearchGetLambda(snes->linesearch, &qn->scaling);CHKERRQ(ierr);
    }

    /* convergence monitoring */
    ierr = PetscInfo4(snes,"fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, lssucceed=%d\n",(double)fnorm,(double)gnorm,(double)ynorm,(int)lssucceed);CHKERRQ(ierr);

    ierr = SNESSetIterationNumber(snes, i+1);CHKERRQ(ierr);
    ierr = SNESSetFunctionNorm(snes, fnorm);CHKERRQ(ierr);

    SNESLogConvHistory(snes,snes->norm,snes->iter);
    ierr = SNESMonitor(snes,snes->iter,snes->norm);CHKERRQ(ierr);
    /* set parameter for default relative tolerance convergence test */
    ierr = (*snes->ops->converged)(snes,snes->iter,0.0,0.0,fnorm,&snes->reason,snes->cnvP);CHKERRQ(ierr);
    if (snes->reason) PetscFunctionReturn(0);


    if (snes->pc) {
      if (qn->compositiontype == SNES_QN_SEQUENTIAL) {
        ierr = SNESSolve(snes->pc, B, X);CHKERRQ(ierr);
        ierr = SNESGetConvergedReason(snes->pc,&reason);CHKERRQ(ierr);
        if (reason < 0 && (reason != SNES_DIVERGED_MAX_IT)) {
          snes->reason = SNES_DIVERGED_INNER;
          PetscFunctionReturn(0);
        }
        ierr = SNESGetFunction(snes->pc, &FPC, PETSC_NULL, PETSC_NULL);CHKERRQ(ierr);
        ierr = VecCopy(FPC, F);CHKERRQ(ierr);
        ierr = SNESGetFunctionNorm(snes->pc, &fnorm);CHKERRQ(ierr);
        ierr = VecCopy(F, D);CHKERRQ(ierr);
      } else {
        ierr = VecCopy(X, D);CHKERRQ(ierr);
        ierr = SNESSolve(snes->pc, B, D);CHKERRQ(ierr);
        ierr = SNESGetConvergedReason(snes->pc,&reason);CHKERRQ(ierr);
        if (reason < 0 && (reason != SNES_DIVERGED_MAX_IT)) {
          snes->reason = SNES_DIVERGED_INNER;
          PetscFunctionReturn(0);
        }
        ierr = VecAYPX(D,-1.0,X);CHKERRQ(ierr);
      }
    } else {
      ierr = VecCopy(F, D);CHKERRQ(ierr);
    }

    /* check restart by Powell's Criterion: |F^T H_0 Fold| > 0.2 * |Fold^T H_0 Fold| */
    ierr = VecDotBegin(Dold, Dold, &DolddotDold);CHKERRQ(ierr);
    ierr = VecDotBegin(Dold, D, &DolddotD);CHKERRQ(ierr);
    ierr = VecDotBegin(D, D, &DdotD);CHKERRQ(ierr);
    ierr = VecDotBegin(Y, D, &YdotD);CHKERRQ(ierr);
    ierr = VecDotEnd(Dold, Dold, &DolddotDold);CHKERRQ(ierr);
    ierr = VecDotEnd(Dold, D, &DolddotD);CHKERRQ(ierr);
    ierr = VecDotEnd(D, D, &DdotD);CHKERRQ(ierr);
    ierr = VecDotEnd(Y, D, &YdotD);CHKERRQ(ierr);
    if (PetscAbs(PetscRealPart(DolddotD)) > qn->powell_gamma*PetscAbs(PetscRealPart(DolddotDold)) || (i_r > qn->n_restart - 1 && qn->n_restart > 0)) {
      if (qn->monitor) {
        ierr = PetscViewerASCIIAddTab(qn->monitor,((PetscObject)snes)->tablevel+2);CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(qn->monitor, "restart! |%14.12e| > %4.2f*|%14.12e| or i_r = %d\n", PetscRealPart(DolddotD), qn->powell_gamma, PetscRealPart(DolddotDold), i_r);CHKERRQ(ierr);
        ierr = PetscViewerASCIISubtractTab(qn->monitor,((PetscObject)snes)->tablevel+2);CHKERRQ(ierr);
      }
      i_r = -1;
      /* general purpose update */
      if (snes->ops->update) {
        ierr = (*snes->ops->update)(snes, snes->iter);CHKERRQ(ierr);
      }
      if (qn->scalingtype == SNES_QN_JACOBIANSCALE) {
        ierr = SNESComputeJacobian(snes,X,&snes->jacobian,&snes->jacobian_pre,&flg);CHKERRQ(ierr);
      }
    } else {
      /* set the differences */
      k = i_r % m;
      l = m;
      if (i_r + 1 < m) l = i_r + 1;
      ierr = VecCopy(D, dF[k]);CHKERRQ(ierr);
      ierr = VecAXPY(dF[k], -1.0, Dold);CHKERRQ(ierr);
      ierr = VecCopy(X, dX[k]);CHKERRQ(ierr);
      ierr = VecAXPY(dX[k], -1.0, Xold);CHKERRQ(ierr);
      if (qn->aggreduct) {
        ierr = VecMDot(dF[k], l, dX, dXtdF);CHKERRQ(ierr);
        ierr = VecMDot(dX[k], l, dF, dFtdX);CHKERRQ(ierr);
        for (j = 0; j < l; j++) {
          H(k, j) = dFtdX[j];
          H(j, k) = dXtdF[j];
        }
        /* copy back over to make the computation of alpha and beta easier */
        for (j = 0; j < l; j++) {
          dXtdF[j] = H(j, j);
        }
      } else {
        ierr = VecDot(dX[k], dF[k], &dXtdF[k]);CHKERRQ(ierr);
      }
      /* set scaling to be shanno scaling */
      if (qn->scalingtype == SNES_QN_SHANNOSCALE) {
        ierr = VecDot(dF[k], dF[k], &a);CHKERRQ(ierr);
        qn->scaling = PetscRealPart(dXtdF[k]) / PetscRealPart(a);
      }
      /* general purpose update */
      if (snes->ops->update) {
        ierr = (*snes->ops->update)(snes, snes->iter);CHKERRQ(ierr);
      }
    }
  }
  if (i == snes->max_its) {
    ierr = PetscInfo1(snes, "Maximum number of iterations has been reached: %D\n", snes->max_its);CHKERRQ(ierr);
    if (!snes->reason) snes->reason = SNES_DIVERGED_MAX_IT;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "SNESSetUp_QN"
static PetscErrorCode SNESSetUp_QN(SNES snes)
{
  SNES_QN        *qn = (SNES_QN*)snes->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = VecDuplicateVecs(snes->vec_sol, qn->m, &qn->dX);CHKERRQ(ierr);
  ierr = VecDuplicateVecs(snes->vec_sol, qn->m, &qn->dF);CHKERRQ(ierr);
  ierr = PetscMalloc3(qn->m, PetscScalar, &qn->alpha,
                      qn->m, PetscScalar, &qn->beta,
                      qn->m, PetscScalar, &qn->dXtdF);CHKERRQ(ierr);

  if (qn->aggreduct) {
    ierr = PetscMalloc3(qn->m*qn->m, PetscScalar, &qn->dXdFmat,
                        qn->m, PetscScalar, &qn->dFtdX,
                        qn->m, PetscScalar, &qn->YtdX);CHKERRQ(ierr);
  }
  ierr = SNESDefaultGetWork(snes,4);CHKERRQ(ierr);

  /* set up the line search */
  if (qn->scalingtype == SNES_QN_JACOBIANSCALE) {
    ierr = SNESSetUpMatrices(snes);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESReset_QN"
static PetscErrorCode SNESReset_QN(SNES snes)
{
  PetscErrorCode ierr;
  SNES_QN *qn;
  PetscFunctionBegin;
  if (snes->data) {
    qn = (SNES_QN*)snes->data;
    if (qn->dX) {
      ierr = VecDestroyVecs(qn->m, &qn->dX);CHKERRQ(ierr);
    }
    if (qn->dF) {
      ierr = VecDestroyVecs(qn->m, &qn->dF);CHKERRQ(ierr);
    }
    if (qn->aggreduct) {
      ierr = PetscFree3(qn->dXdFmat, qn->dFtdX, qn->YtdX);CHKERRQ(ierr);
    }
    ierr = PetscFree3(qn->alpha, qn->beta, qn->dXtdF);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESDestroy_QN"
static PetscErrorCode SNESDestroy_QN(SNES snes)
{
  PetscErrorCode ierr;
  PetscFunctionBegin;
  ierr = SNESReset_QN(snes);CHKERRQ(ierr);
  ierr = PetscFree(snes->data);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)snes,"","",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SNESSetFromOptions_QN"
static PetscErrorCode SNESSetFromOptions_QN(SNES snes)
{

  PetscErrorCode ierr;
  SNES_QN    *qn;
  const char *compositions[] = {"sequential", "composed"};
  const char *scalings[]     = {"shanno", "ls", "jacobian"};
  PetscInt   indx = 0;
  PetscBool  flg;
  PetscBool  monflg = PETSC_FALSE;
  SNESLineSearch linesearch;
  PetscFunctionBegin;

  qn = (SNES_QN*)snes->data;

  ierr = PetscOptionsHead("SNES QN options");CHKERRQ(ierr);
  ierr = PetscOptionsInt("-snes_qn_m",                "Number of past states saved for L-BFGS methods", "SNESQN", qn->m, &qn->m, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-snes_qn_restart",                "Maximum number of iterations between restarts", "SNESQN", qn->n_restart, &qn->n_restart, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-snes_qn_powell_gamma",    "Powell angle tolerance",          "SNESQN", qn->powell_gamma, &qn->powell_gamma, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-snes_qn_powell_downhill", "Powell descent tolerance",        "SNESQN", qn->powell_downhill, &qn->powell_downhill, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-snes_qn_monitor",         "Monitor for the QN methods",      "SNESQN", monflg, &monflg, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-snes_qn_aggreduct",       "Aggregate reductions",            "SNESQN", qn->aggreduct, &qn->aggreduct, PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEList("-snes_qn_composition",    "Composition type",                "SNESQN",compositions,2,"sequential",&indx,&flg);CHKERRQ(ierr);
  if (flg) {
    switch (indx) {
    case 0: qn->compositiontype = SNES_QN_SEQUENTIAL;
      break;
    case 1: qn->compositiontype = SNES_QN_COMPOSED;
      break;
    }
  }
  ierr = PetscOptionsEList("-snes_qn_scaling",        "Scaling type",                    "SNESQN",scalings,3,"shanno",&indx,&flg);CHKERRQ(ierr);
  if (flg) {
    switch (indx) {
    case 0: qn->scalingtype = SNES_QN_SHANNOSCALE;
      break;
    case 1: qn->scalingtype = SNES_QN_LSSCALE;
      break;
    case 2: qn->scalingtype = SNES_QN_JACOBIANSCALE;
      snes->usesksp = PETSC_TRUE;
      break;
    }
  }

  ierr = PetscOptionsTail();CHKERRQ(ierr);
  if (!snes->linesearch) {
    ierr = SNESGetSNESLineSearch(snes, &linesearch);CHKERRQ(ierr);
    if (!snes->pc || qn->compositiontype == SNES_QN_SEQUENTIAL) {
      ierr = SNESLineSearchSetType(linesearch, SNES_LINESEARCH_CP);CHKERRQ(ierr);
    } else {
      ierr = SNESLineSearchSetType(linesearch, SNES_LINESEARCH_L2);CHKERRQ(ierr);
    }
  }
  if (monflg) {
    qn->monitor = PETSC_VIEWER_STDOUT_(((PetscObject)snes)->comm);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


/* -------------------------------------------------------------------------- */
/*MC
      SNESQN - Limited-Memory Quasi-Newton methods for the solution of nonlinear systems.

      Options Database:

+     -snes_qn_m - Number of past states saved for the L-Broyden methods.
.     -snes_qn_powell_angle - Angle condition for restart.
.     -snes_qn_powell_descent - Descent condition for restart.
.     -snes_qn_composition <sequential, composed>- Type of composition.
.     -snes_ls <basic, basicnonorms, quadratic, critical> - Type of line search.
-     -snes_qn_monitor - Monitors the quasi-newton jacobian.

      Notes: This implements the L-BFGS algorithm for the solution of F(x) = b using previous change in F(x) and x to
      form the approximate inverse Jacobian using a series of multiplicative rank-one updates.  This will eventually be
      generalized to implement several limited-memory Broyden methods.

      When using a nonlinear preconditioner, one has two options as to how the preconditioner is applied.  The first of
      these options, sequential, uses the preconditioner to generate a new solution and function and uses those at this
      iteration as the current iteration's values when constructing the approximate jacobian.  The second, composed,
      perturbs the problem the jacobian represents to be P(x, b) - x = 0, where P(x, b) is the preconditioner.

      References:

      L-Broyden Methods: a generalization of the L-BFGS method to the limited memory Broyden family, M. B. Reed,
      International Journal of Computer Mathematics, vol. 86, 2009.


      Level: beginner

.seealso:  SNESCreate(), SNES, SNESSetType(), SNESLS, SNESTR

M*/
EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "SNESCreate_QN"
PetscErrorCode  SNESCreate_QN(SNES snes)
{

  PetscErrorCode ierr;
  SNES_QN *qn;

  PetscFunctionBegin;
  snes->ops->setup           = SNESSetUp_QN;
  snes->ops->solve           = SNESSolve_QN;
  snes->ops->destroy         = SNESDestroy_QN;
  snes->ops->setfromoptions  = SNESSetFromOptions_QN;
  snes->ops->view            = 0;
  snes->ops->reset           = SNESReset_QN;

  snes->usespc          = PETSC_TRUE;
  snes->usesksp         = PETSC_FALSE;

  if (!snes->tolerancesset) {
    snes->max_funcs = 30000;
    snes->max_its   = 10000;
  }

  ierr = PetscNewLog(snes,SNES_QN,&qn);CHKERRQ(ierr);
  snes->data = (void *) qn;
  qn->m               = 10;
  qn->scaling         = 1.0;
  qn->dX              = PETSC_NULL;
  qn->dF              = PETSC_NULL;
  qn->dXtdF           = PETSC_NULL;
  qn->dFtdX           = PETSC_NULL;
  qn->dXdFmat         = PETSC_NULL;
  qn->monitor         = PETSC_NULL;
  qn->aggreduct       = PETSC_FALSE;
  qn->powell_gamma    = 0.9;
  qn->powell_downhill = 0.2;
  qn->compositiontype = SNES_QN_SEQUENTIAL;
  qn->scalingtype     = SNES_QN_SHANNOSCALE;
  qn->n_restart       = -1;

  PetscFunctionReturn(0);
}

EXTERN_C_END
