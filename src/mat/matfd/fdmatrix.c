/*$Id: fdmatrix.c,v 1.92 2001/08/21 21:03:06 bsmith Exp $*/

/*
   This is where the abstract matrix operations are defined that are
  used for finite difference computations of Jacobians using coloring.
*/

#include "petsc.h"
#include "src/mat/matimpl.h"        /*I "petscmat.h" I*/

/* Logging support */
int MAT_FDCOLORING_COOKIE;

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringSetF"
int MatFDColoringSetF(MatFDColoring fd,Vec F)
{
  PetscFunctionBegin;
  fd->F = F;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringView_Draw_Zoom"
static int MatFDColoringView_Draw_Zoom(PetscDraw draw,void *Aa)
{
  MatFDColoring fd = (MatFDColoring)Aa;
  int           ierr,i,j;
  PetscReal     x,y;

  PetscFunctionBegin;

  /* loop over colors  */
  for (i=0; i<fd->ncolors; i++) {
    for (j=0; j<fd->nrows[i]; j++) {
      y = fd->M - fd->rows[i][j] - fd->rstart;
      x = fd->columnsforrow[i][j];
      ierr = PetscDrawRectangle(draw,x,y,x+1,y+1,i+1,i+1,i+1,i+1);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringView_Draw"
static int MatFDColoringView_Draw(MatFDColoring fd,PetscViewer viewer)
{
  int         ierr;
  PetscTruth  isnull;
  PetscDraw   draw;
  PetscReal   xr,yr,xl,yl,h,w;

  PetscFunctionBegin;
  ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
  ierr = PetscDrawIsNull(draw,&isnull);CHKERRQ(ierr); if (isnull) PetscFunctionReturn(0);

  ierr = PetscObjectCompose((PetscObject)fd,"Zoomviewer",(PetscObject)viewer);CHKERRQ(ierr);

  xr  = fd->N; yr = fd->M; h = yr/10.0; w = xr/10.0; 
  xr += w;     yr += h;    xl = -w;     yl = -h;
  ierr = PetscDrawSetCoordinates(draw,xl,yl,xr,yr);CHKERRQ(ierr);
  ierr = PetscDrawZoom(draw,MatFDColoringView_Draw_Zoom,fd);CHKERRQ(ierr);
  ierr = PetscObjectCompose((PetscObject)fd,"Zoomviewer",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringView"
/*@C
   MatFDColoringView - Views a finite difference coloring context.

   Collective on MatFDColoring

   Input  Parameters:
+  c - the coloring context
-  viewer - visualization context

   Level: intermediate

   Notes:
   The available visualization contexts include
+     PETSC_VIEWER_STDOUT_SELF - standard output (default)
.     PETSC_VIEWER_STDOUT_WORLD - synchronized standard
        output where only the first processor opens
        the file.  All other processors send their 
        data to the first processor to print. 
-     PETSC_VIEWER_DRAW_WORLD - graphical display of nonzero structure

   Notes:
     Since PETSc uses only a small number of basic colors (currently 33), if the coloring
   involves more than 33 then some seemingly identical colors are displayed making it look
   like an illegal coloring. This is just a graphical artifact.

.seealso: MatFDColoringCreate()

.keywords: Mat, finite differences, coloring, view
@*/
int MatFDColoringView(MatFDColoring c,PetscViewer viewer)
{
  int               i,j,ierr;
  PetscTruth        isdraw,isascii;
  PetscViewerFormat format;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(c,MAT_FDCOLORING_COOKIE);
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(c->comm);
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE); 
  PetscCheckSameComm(c,viewer);

  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_DRAW,&isdraw);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isdraw) { 
    ierr = MatFDColoringView_Draw(c,viewer);CHKERRQ(ierr);
  } else if (isascii) {
    ierr = PetscViewerASCIIPrintf(viewer,"MatFDColoring Object:\n");CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  Error tolerance=%g\n",c->error_rel);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  Umin=%g\n",c->umin);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  Number of colors=%d\n",c->ncolors);CHKERRQ(ierr);

    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    if (format != PETSC_VIEWER_ASCII_INFO) {
      for (i=0; i<c->ncolors; i++) {
        ierr = PetscViewerASCIIPrintf(viewer,"  Information for color %d\n",i);CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer,"    Number of columns %d\n",c->ncolumns[i]);CHKERRQ(ierr);
        for (j=0; j<c->ncolumns[i]; j++) {
          ierr = PetscViewerASCIIPrintf(viewer,"      %d\n",c->columns[i][j]);CHKERRQ(ierr);
        }
        ierr = PetscViewerASCIIPrintf(viewer,"    Number of rows %d\n",c->nrows[i]);CHKERRQ(ierr);
        for (j=0; j<c->nrows[i]; j++) {
          ierr = PetscViewerASCIIPrintf(viewer,"      %d %d \n",c->rows[i][j],c->columnsforrow[i][j]);CHKERRQ(ierr);
        }
      }
    }
    ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  } else {
    SETERRQ1(1,"Viewer type %s not supported for MatFDColoring",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringSetParameters"
/*@
   MatFDColoringSetParameters - Sets the parameters for the sparse approximation of
   a Jacobian matrix using finite differences.

   Collective on MatFDColoring

   The Jacobian is estimated with the differencing approximation
.vb
       F'(u)_{:,i} = [F(u+h*dx_{i}) - F(u)]/h where
       h = error_rel*u[i]                 if  abs(u[i]) > umin
         = +/- error_rel*umin             otherwise, with +/- determined by the sign of u[i]
       dx_{i} = (0, ... 1, .... 0)
.ve

   Input Parameters:
+  coloring - the coloring context
.  error_rel - relative error
-  umin - minimum allowable u-value magnitude

   Level: advanced

.keywords: Mat, finite differences, coloring, set, parameters

.seealso: MatFDColoringCreate()
@*/
int MatFDColoringSetParameters(MatFDColoring matfd,PetscReal error,PetscReal umin)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(matfd,MAT_FDCOLORING_COOKIE);

  if (error != PETSC_DEFAULT) matfd->error_rel = error;
  if (umin != PETSC_DEFAULT)  matfd->umin      = umin;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringSetFrequency"
/*@
   MatFDColoringSetFrequency - Sets the frequency for computing new Jacobian
   matrices. 

   Collective on MatFDColoring

   Input Parameters:
+  coloring - the coloring context
-  freq - frequency (default is 1)

   Options Database Keys:
.  -mat_fd_coloring_freq <freq>  - Sets coloring frequency

   Level: advanced

   Notes:
   Using a modified Newton strategy, where the Jacobian remains fixed for several
   iterations, can be cost effective in terms of overall nonlinear solution 
   efficiency.  This parameter indicates that a new Jacobian will be computed every
   <freq> nonlinear iterations.  

.keywords: Mat, finite differences, coloring, set, frequency

.seealso: MatFDColoringCreate(), MatFDColoringGetFrequency(), MatFDColoringSetRecompute()
@*/
int MatFDColoringSetFrequency(MatFDColoring matfd,int freq)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(matfd,MAT_FDCOLORING_COOKIE);

  matfd->freq = freq;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringGetFrequency"
/*@
   MatFDColoringGetFrequency - Gets the frequency for computing new Jacobian
   matrices. 

   Not Collective

   Input Parameters:
.  coloring - the coloring context

   Output Parameters:
.  freq - frequency (default is 1)

   Options Database Keys:
.  -mat_fd_coloring_freq <freq> - Sets coloring frequency

   Level: advanced

   Notes:
   Using a modified Newton strategy, where the Jacobian remains fixed for several
   iterations, can be cost effective in terms of overall nonlinear solution 
   efficiency.  This parameter indicates that a new Jacobian will be computed every
   <freq> nonlinear iterations.  

.keywords: Mat, finite differences, coloring, get, frequency

.seealso: MatFDColoringSetFrequency()
@*/
int MatFDColoringGetFrequency(MatFDColoring matfd,int *freq)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(matfd,MAT_FDCOLORING_COOKIE);

  *freq = matfd->freq;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringSetFunction"
/*@C
   MatFDColoringSetFunction - Sets the function to use for computing the Jacobian.

   Collective on MatFDColoring

   Input Parameters:
+  coloring - the coloring context
.  f - the function
-  fctx - the optional user-defined function context

   Level: intermediate

   Notes:
    In Fortran you must call MatFDColoringSetFunctionSNES() for a coloring object to 
  be used with the SNES solvers and MatFDColoringSetFunctionTS() if it is to be used
  with the TS solvers.

.keywords: Mat, Jacobian, finite differences, set, function
@*/
int MatFDColoringSetFunction(MatFDColoring matfd,int (*f)(void),void *fctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(matfd,MAT_FDCOLORING_COOKIE);

  matfd->f    = f;
  matfd->fctx = fctx;

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringSetFromOptions"
/*@
   MatFDColoringSetFromOptions - Sets coloring finite difference parameters from 
   the options database.

   Collective on MatFDColoring

   The Jacobian, F'(u), is estimated with the differencing approximation
.vb
       F'(u)_{:,i} = [F(u+h*dx_{i}) - F(u)]/h where
       h = error_rel*u[i]                 if  abs(u[i]) > umin
         = +/- error_rel*umin             otherwise, with +/- determined by the sign of u[i]
       dx_{i} = (0, ... 1, .... 0)
.ve

   Input Parameter:
.  coloring - the coloring context

   Options Database Keys:
+  -mat_fd_coloring_err <err> - Sets <err> (square root
           of relative error in the function)
.  -mat_fd_coloring_umin <umin> - Sets umin, the minimum allowable u-value magnitude
.  -mat_fd_coloring_freq <freq> - Sets frequency of computing a new Jacobian
.  -mat_fd_coloring_view - Activates basic viewing
.  -mat_fd_coloring_view_info - Activates viewing info
-  -mat_fd_coloring_view_draw - Activates drawing

    Level: intermediate

.keywords: Mat, finite differences, parameters

.seealso: MatFDColoringCreate(), MatFDColoringView(), MatFDColoringSetParameters()

@*/
int MatFDColoringSetFromOptions(MatFDColoring matfd)
{
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(matfd,MAT_FDCOLORING_COOKIE);

  ierr = PetscOptionsBegin(matfd->comm,matfd->prefix,"Jacobian computation via finite differences option","MatFD");CHKERRQ(ierr);
    ierr = PetscOptionsReal("-mat_fd_coloring_err","Square root of relative error in function","MatFDColoringSetParameters",matfd->error_rel,&matfd->error_rel,0);CHKERRQ(ierr);
    ierr = PetscOptionsReal("-mat_fd_coloring_umin","Minimum allowable u magnitude","MatFDColoringSetParameters",matfd->umin,&matfd->umin,0);CHKERRQ(ierr);
    ierr = PetscOptionsInt("-mat_fd_coloring_freq","How often Jacobian is recomputed","MatFDColoringSetFrequency",matfd->freq,&matfd->freq,0);CHKERRQ(ierr);
    /* not used here; but so they are presented in the GUI */
    ierr = PetscOptionsName("-mat_fd_coloring_view","Print entire datastructure used for Jacobian","None",0);CHKERRQ(ierr);
    ierr = PetscOptionsName("-mat_fd_coloring_view_info","Print number of colors etc for Jacobian","None",0);CHKERRQ(ierr);
    ierr = PetscOptionsName("-mat_fd_coloring_view_draw","Plot nonzero structure ofJacobian","None",0);CHKERRQ(ierr);
  PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringView_Private"
int MatFDColoringView_Private(MatFDColoring fd)
{
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_fd_coloring_view",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MatFDColoringView(fd,PETSC_VIEWER_STDOUT_(fd->comm));CHKERRQ(ierr);
  }
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_fd_coloring_view_info",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscViewerPushFormat(PETSC_VIEWER_STDOUT_(fd->comm),PETSC_VIEWER_ASCII_INFO);CHKERRQ(ierr);
    ierr = MatFDColoringView(fd,PETSC_VIEWER_STDOUT_(fd->comm));CHKERRQ(ierr);
    ierr = PetscViewerPopFormat(PETSC_VIEWER_STDOUT_(fd->comm));CHKERRQ(ierr);
  }
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_fd_coloring_view_draw",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MatFDColoringView(fd,PETSC_VIEWER_DRAW_(fd->comm));CHKERRQ(ierr);
    ierr = PetscViewerFlush(PETSC_VIEWER_DRAW_(fd->comm));CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringCreate" 
/*@C
   MatFDColoringCreate - Creates a matrix coloring context for finite difference 
   computation of Jacobians.

   Collective on Mat

   Input Parameters:
+  mat - the matrix containing the nonzero structure of the Jacobian
-  iscoloring - the coloring of the matrix

    Output Parameter:
.   color - the new coloring context
   
    Options Database Keys:
+    -mat_fd_coloring_view - Activates basic viewing or coloring
.    -mat_fd_coloring_view_draw - Activates drawing of coloring
-    -mat_fd_coloring_view_info - Activates viewing of coloring info

    Level: intermediate

.seealso: MatFDColoringDestroy(),SNESDefaultComputeJacobianColor(), ISColoringCreate(),
          MatFDColoringSetFunction(), MatFDColoringSetFromOptions(), MatFDColoringApply(),
          MatFDColoringSetFrequency(), MatFDColoringSetRecompute(), MatFDColoringView(),
          MatFDColoringSetParameters()
@*/
int MatFDColoringCreate(Mat mat,ISColoring iscoloring,MatFDColoring *color)
{
  MatFDColoring c;
  MPI_Comm      comm;
  int           ierr,M,N;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(MAT_FDColoringCreate,mat,0,0,0);CHKERRQ(ierr);
  ierr = MatGetSize(mat,&M,&N);CHKERRQ(ierr);
  if (M != N) SETERRQ(PETSC_ERR_SUP,"Only for square matrices");

  ierr = PetscObjectGetComm((PetscObject)mat,&comm);CHKERRQ(ierr);
  PetscHeaderCreate(c,_p_MatFDColoring,int,MAT_FDCOLORING_COOKIE,0,"MatFDColoring",comm,MatFDColoringDestroy,MatFDColoringView);
  PetscLogObjectCreate(c);

  if (mat->ops->fdcoloringcreate) {
    ierr = (*mat->ops->fdcoloringcreate)(mat,iscoloring,c);CHKERRQ(ierr);
  } else {
    SETERRQ(PETSC_ERR_SUP,"Code not yet written for this matrix type");
  }

  c->error_rel         = PETSC_SQRT_MACHINE_EPSILON;
  c->umin              = 100.0*PETSC_SQRT_MACHINE_EPSILON;
  c->freq              = 1;
  c->usersetsrecompute = PETSC_FALSE;
  c->recompute         = PETSC_FALSE;

  ierr = MatFDColoringView_Private(c);CHKERRQ(ierr);

  *color = c;
  ierr = PetscLogEventEnd(MAT_FDColoringCreate,mat,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringDestroy"
/*@C
    MatFDColoringDestroy - Destroys a matrix coloring context that was created
    via MatFDColoringCreate().

    Collective on MatFDColoring

    Input Parameter:
.   c - coloring context

    Level: intermediate

.seealso: MatFDColoringCreate()
@*/
int MatFDColoringDestroy(MatFDColoring c)
{
  int i,ierr;

  PetscFunctionBegin;
  if (--c->refct > 0) PetscFunctionReturn(0);

  for (i=0; i<c->ncolors; i++) {
    if (c->columns[i])         {ierr = PetscFree(c->columns[i]);CHKERRQ(ierr);}
    if (c->rows[i])            {ierr = PetscFree(c->rows[i]);CHKERRQ(ierr);}
    if (c->columnsforrow[i])   {ierr = PetscFree(c->columnsforrow[i]);CHKERRQ(ierr);}
    if (c->vscaleforrow && c->vscaleforrow[i]) {ierr = PetscFree(c->vscaleforrow[i]);CHKERRQ(ierr);} 
  }
  ierr = PetscFree(c->ncolumns);CHKERRQ(ierr);
  ierr = PetscFree(c->columns);CHKERRQ(ierr);
  ierr = PetscFree(c->nrows);CHKERRQ(ierr);
  ierr = PetscFree(c->rows);CHKERRQ(ierr);
  ierr = PetscFree(c->columnsforrow);CHKERRQ(ierr);
  if (c->vscaleforrow) {ierr = PetscFree(c->vscaleforrow);CHKERRQ(ierr);}
  if (c->vscale)       {ierr = VecDestroy(c->vscale);CHKERRQ(ierr);}
  if (c->w1) {
    ierr = VecDestroy(c->w1);CHKERRQ(ierr);
    ierr = VecDestroy(c->w2);CHKERRQ(ierr);
    ierr = VecDestroy(c->w3);CHKERRQ(ierr);
  }
  PetscLogObjectDestroy(c);
  PetscHeaderDestroy(c);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringApply"
/*@
    MatFDColoringApply - Given a matrix for which a MatFDColoring context 
    has been created, computes the Jacobian for a function via finite differences.

    Collective on MatFDColoring

    Input Parameters:
+   mat - location to store Jacobian
.   coloring - coloring context created with MatFDColoringCreate()
.   x1 - location at which Jacobian is to be computed
-   sctx - optional context required by function (actually a SNES context)

   Options Database Keys:
.  -mat_fd_coloring_freq <freq> - Sets coloring frequency

   Level: intermediate

.seealso: MatFDColoringCreate(), MatFDColoringDestroy(), MatFDColoringView()

.keywords: coloring, Jacobian, finite differences
@*/
int MatFDColoringApply(Mat J,MatFDColoring coloring,Vec x1,MatStructure *flag,void *sctx)
{
  int           (*f)(void *,Vec,Vec,void*) = (int (*)(void *,Vec,Vec,void *))coloring->f;
  int           k,ierr,N,start,end,l,row,col,srow,**vscaleforrow,m1,m2;
  PetscScalar   dx,mone = -1.0,*y,*xx,*w3_array;
  PetscScalar   *vscale_array;
  PetscReal     epsilon = coloring->error_rel,umin = coloring->umin; 
  Vec           w1,w2,w3;
  void          *fctx = coloring->fctx;
  PetscTruth    flg;


  PetscFunctionBegin;
  PetscValidHeaderSpecific(J,MAT_COOKIE);
  PetscValidHeaderSpecific(coloring,MAT_FDCOLORING_COOKIE);
  PetscValidHeaderSpecific(x1,VEC_COOKIE);

  if (coloring->usersetsrecompute) {
    if (!coloring->recompute) {
      *flag = SAME_PRECONDITIONER;
      PetscLogInfo(J,"MatFDColoringApply:Skipping Jacobian, since user called MatFDColorSetRecompute()\n");
      PetscFunctionReturn(0);
    } else {
      coloring->recompute = PETSC_FALSE;
    }
  }

  ierr = PetscLogEventBegin(MAT_FDColoringApply,coloring,J,x1,0);CHKERRQ(ierr);
  if (J->ops->fdcoloringapply) {
    ierr = (*J->ops->fdcoloringapply)(J,coloring,x1,flag,sctx);CHKERRQ(ierr);
  } else {

    if (!coloring->w1) {
      ierr = VecDuplicate(x1,&coloring->w1);CHKERRQ(ierr);
      PetscLogObjectParent(coloring,coloring->w1);
      ierr = VecDuplicate(x1,&coloring->w2);CHKERRQ(ierr);
      PetscLogObjectParent(coloring,coloring->w2);
      ierr = VecDuplicate(x1,&coloring->w3);CHKERRQ(ierr);
      PetscLogObjectParent(coloring,coloring->w3);
    }
    w1 = coloring->w1; w2 = coloring->w2; w3 = coloring->w3;

    ierr = MatSetUnfactored(J);CHKERRQ(ierr);
    ierr = PetscOptionsHasName(PETSC_NULL,"-mat_fd_coloring_dont_rezero",&flg);CHKERRQ(ierr);
    if (flg) {
      PetscLogInfo(coloring,"MatFDColoringApply: Not calling MatZeroEntries()\n");
    } else {
      ierr = MatZeroEntries(J);CHKERRQ(ierr);
    }

    ierr = VecGetOwnershipRange(x1,&start,&end);CHKERRQ(ierr);
    ierr = VecGetSize(x1,&N);CHKERRQ(ierr);
    
    /*
      This is a horrible, horrible, hack. See DMMGComputeJacobian_Multigrid() it inproperly sets
      coloring->F for the coarser grids from the finest
    */
    if (coloring->F) {
      ierr = VecGetLocalSize(coloring->F,&m1);CHKERRQ(ierr);
      ierr = VecGetLocalSize(w1,&m2);CHKERRQ(ierr);
      if (m1 != m2) {
	coloring->F = 0; 
      }    
    }

    if (coloring->F) {
      w1          = coloring->F; /* use already computed value of function */
      coloring->F = 0; 
    } else {
      ierr = (*f)(sctx,x1,w1,fctx);CHKERRQ(ierr);
    }

    /* 
       Compute all the scale factors and share with other processors
    */
    ierr = VecGetArray(x1,&xx);CHKERRQ(ierr);xx = xx - start;
    ierr = VecGetArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);vscale_array = vscale_array - start;
    for (k=0; k<coloring->ncolors; k++) { 
      /*
	Loop over each column associated with color adding the 
	perturbation to the vector w3.
      */
      for (l=0; l<coloring->ncolumns[k]; l++) {
	col = coloring->columns[k][l];    /* column of the matrix we are probing for */
	dx  = xx[col];
	if (dx == 0.0) dx = 1.0;
#if !defined(PETSC_USE_COMPLEX)
	if (dx < umin && dx >= 0.0)      dx = umin;
	else if (dx < 0.0 && dx > -umin) dx = -umin;
#else
	if (PetscAbsScalar(dx) < umin && PetscRealPart(dx) >= 0.0)     dx = umin;
	else if (PetscRealPart(dx) < 0.0 && PetscAbsScalar(dx) < umin) dx = -umin;
#endif
	dx                *= epsilon;
	vscale_array[col] = 1.0/dx;
      }
    } 
    vscale_array = vscale_array + start;ierr = VecRestoreArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);
    ierr = VecGhostUpdateBegin(coloring->vscale,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
    ierr = VecGhostUpdateEnd(coloring->vscale,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);

    /*  ierr = VecView(coloring->vscale,PETSC_VIEWER_STDOUT_WORLD);
	ierr = VecView(x1,PETSC_VIEWER_STDOUT_WORLD);*/

    if (coloring->vscaleforrow) vscaleforrow = coloring->vscaleforrow;
    else                        vscaleforrow = coloring->columnsforrow;

    ierr = VecGetArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);
    /*
      Loop over each color
    */
    for (k=0; k<coloring->ncolors; k++) { 
      ierr = VecCopy(x1,w3);CHKERRQ(ierr);
      ierr = VecGetArray(w3,&w3_array);CHKERRQ(ierr);w3_array = w3_array - start;
      /*
	Loop over each column associated with color adding the 
	perturbation to the vector w3.
      */
      for (l=0; l<coloring->ncolumns[k]; l++) {
	col = coloring->columns[k][l];    /* column of the matrix we are probing for */
	dx  = xx[col];
	if (dx == 0.0) dx = 1.0;
#if !defined(PETSC_USE_COMPLEX)
	if (dx < umin && dx >= 0.0)      dx = umin;
	else if (dx < 0.0 && dx > -umin) dx = -umin;
#else
	if (PetscAbsScalar(dx) < umin && PetscRealPart(dx) >= 0.0)     dx = umin;
	else if (PetscRealPart(dx) < 0.0 && PetscAbsScalar(dx) < umin) dx = -umin;
#endif
	dx            *= epsilon;
	if (!PetscAbsScalar(dx)) SETERRQ(1,"Computed 0 differencing parameter");
	w3_array[col] += dx;
      } 
      w3_array = w3_array + start; ierr = VecRestoreArray(w3,&w3_array);CHKERRQ(ierr);

      /*
	Evaluate function at x1 + dx (here dx is a vector of perturbations)
      */

      ierr = (*f)(sctx,w3,w2,fctx);CHKERRQ(ierr);
      ierr = VecAXPY(&mone,w1,w2);CHKERRQ(ierr);

      /*
	Loop over rows of vector, putting results into Jacobian matrix
      */
      ierr = VecGetArray(w2,&y);CHKERRQ(ierr);
      for (l=0; l<coloring->nrows[k]; l++) {
	row    = coloring->rows[k][l];
	col    = coloring->columnsforrow[k][l];
	y[row] *= vscale_array[vscaleforrow[k][l]];
	srow   = row + start;
	ierr   = MatSetValues(J,1,&srow,1,&col,y+row,INSERT_VALUES);CHKERRQ(ierr);
      }
      ierr = VecRestoreArray(w2,&y);CHKERRQ(ierr);
    }
    ierr = VecRestoreArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);
    xx = xx + start; ierr  = VecRestoreArray(x1,&xx);CHKERRQ(ierr);
    ierr  = MatAssemblyBegin(J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr  = MatAssemblyEnd(J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  ierr = PetscLogEventEnd(MAT_FDColoringApply,coloring,J,x1,0);CHKERRQ(ierr);

  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_null_space_test",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MatNullSpaceTest(J->nullsp,J);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringApplyTS"
/*@
    MatFDColoringApplyTS - Given a matrix for which a MatFDColoring context 
    has been created, computes the Jacobian for a function via finite differences.

   Collective on Mat, MatFDColoring, and Vec

    Input Parameters:
+   mat - location to store Jacobian
.   coloring - coloring context created with MatFDColoringCreate()
.   x1 - location at which Jacobian is to be computed
-   sctx - optional context required by function (actually a SNES context)

   Options Database Keys:
.  -mat_fd_coloring_freq <freq> - Sets coloring frequency

   Level: intermediate

.seealso: MatFDColoringCreate(), MatFDColoringDestroy(), MatFDColoringView()

.keywords: coloring, Jacobian, finite differences
@*/
int MatFDColoringApplyTS(Mat J,MatFDColoring coloring,PetscReal t,Vec x1,MatStructure *flag,void *sctx)
{
  int           (*f)(void *,PetscReal,Vec,Vec,void*)=(int (*)(void *,PetscReal,Vec,Vec,void *))coloring->f;
  int           k,ierr,N,start,end,l,row,col,srow,**vscaleforrow;
  PetscScalar   dx,mone = -1.0,*y,*xx,*w3_array;
  PetscScalar   *vscale_array;
  PetscReal     epsilon = coloring->error_rel,umin = coloring->umin; 
  Vec           w1,w2,w3;
  void          *fctx = coloring->fctx;
  PetscTruth    flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(J,MAT_COOKIE);
  PetscValidHeaderSpecific(coloring,MAT_FDCOLORING_COOKIE);
  PetscValidHeaderSpecific(x1,VEC_COOKIE);

  ierr = PetscLogEventBegin(MAT_FDColoringApply,coloring,J,x1,0);CHKERRQ(ierr);
  if (!coloring->w1) {
    ierr = VecDuplicate(x1,&coloring->w1);CHKERRQ(ierr);
    PetscLogObjectParent(coloring,coloring->w1);
    ierr = VecDuplicate(x1,&coloring->w2);CHKERRQ(ierr);
    PetscLogObjectParent(coloring,coloring->w2);
    ierr = VecDuplicate(x1,&coloring->w3);CHKERRQ(ierr);
    PetscLogObjectParent(coloring,coloring->w3);
  }
  w1 = coloring->w1; w2 = coloring->w2; w3 = coloring->w3;

  ierr = MatSetUnfactored(J);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_fd_coloring_dont_rezero",&flg);CHKERRQ(ierr);
  if (flg) {
    PetscLogInfo(coloring,"MatFDColoringApply: Not calling MatZeroEntries()\n");
  } else {
    ierr = MatZeroEntries(J);CHKERRQ(ierr);
  }

  ierr = VecGetOwnershipRange(x1,&start,&end);CHKERRQ(ierr);
  ierr = VecGetSize(x1,&N);CHKERRQ(ierr);
  ierr = (*f)(sctx,t,x1,w1,fctx);CHKERRQ(ierr);

  /* 
      Compute all the scale factors and share with other processors
  */
  ierr = VecGetArray(x1,&xx);CHKERRQ(ierr);xx = xx - start;
  ierr = VecGetArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);vscale_array = vscale_array - start;
  for (k=0; k<coloring->ncolors; k++) { 
    /*
       Loop over each column associated with color adding the 
       perturbation to the vector w3.
    */
    for (l=0; l<coloring->ncolumns[k]; l++) {
      col = coloring->columns[k][l];    /* column of the matrix we are probing for */
      dx  = xx[col];
      if (dx == 0.0) dx = 1.0;
#if !defined(PETSC_USE_COMPLEX)
      if (dx < umin && dx >= 0.0)      dx = umin;
      else if (dx < 0.0 && dx > -umin) dx = -umin;
#else
      if (PetscAbsScalar(dx) < umin && PetscRealPart(dx) >= 0.0)     dx = umin;
      else if (PetscRealPart(dx) < 0.0 && PetscAbsScalar(dx) < umin) dx = -umin;
#endif
      dx                *= epsilon;
      vscale_array[col] = 1.0/dx;
    }
  } 
  vscale_array = vscale_array - start;ierr = VecRestoreArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);
  ierr = VecGhostUpdateBegin(coloring->vscale,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);
  ierr = VecGhostUpdateEnd(coloring->vscale,INSERT_VALUES,SCATTER_FORWARD);CHKERRQ(ierr);

  if (coloring->vscaleforrow) vscaleforrow = coloring->vscaleforrow;
  else                        vscaleforrow = coloring->columnsforrow;

  ierr = VecGetArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);
  /*
      Loop over each color
  */
  for (k=0; k<coloring->ncolors; k++) { 
    ierr = VecCopy(x1,w3);CHKERRQ(ierr);
    ierr = VecGetArray(w3,&w3_array);CHKERRQ(ierr);w3_array = w3_array - start;
    /*
       Loop over each column associated with color adding the 
       perturbation to the vector w3.
    */
    for (l=0; l<coloring->ncolumns[k]; l++) {
      col = coloring->columns[k][l];    /* column of the matrix we are probing for */
      dx  = xx[col];
      if (dx == 0.0) dx = 1.0;
#if !defined(PETSC_USE_COMPLEX)
      if (dx < umin && dx >= 0.0)      dx = umin;
      else if (dx < 0.0 && dx > -umin) dx = -umin;
#else
      if (PetscAbsScalar(dx) < umin && PetscRealPart(dx) >= 0.0)     dx = umin;
      else if (PetscRealPart(dx) < 0.0 && PetscAbsScalar(dx) < umin) dx = -umin;
#endif
      dx            *= epsilon;
      w3_array[col] += dx;
    } 
    w3_array = w3_array + start; ierr = VecRestoreArray(w3,&w3_array);CHKERRQ(ierr);

    /*
       Evaluate function at x1 + dx (here dx is a vector of perturbations)
    */
    ierr = (*f)(sctx,t,w3,w2,fctx);CHKERRQ(ierr);
    ierr = VecAXPY(&mone,w1,w2);CHKERRQ(ierr);

    /*
       Loop over rows of vector, putting results into Jacobian matrix
    */
    ierr = VecGetArray(w2,&y);CHKERRQ(ierr);
    for (l=0; l<coloring->nrows[k]; l++) {
      row    = coloring->rows[k][l];
      col    = coloring->columnsforrow[k][l];
      y[row] *= vscale_array[vscaleforrow[k][l]];
      srow   = row + start;
      ierr   = MatSetValues(J,1,&srow,1,&col,y+row,INSERT_VALUES);CHKERRQ(ierr);
    }
    ierr = VecRestoreArray(w2,&y);CHKERRQ(ierr);
  }
  ierr  = VecRestoreArray(coloring->vscale,&vscale_array);CHKERRQ(ierr);
  xx    = xx + start; ierr  = VecRestoreArray(x1,&xx);CHKERRQ(ierr);
  ierr  = MatAssemblyBegin(J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr  = MatAssemblyEnd(J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr  = PetscLogEventEnd(MAT_FDColoringApply,coloring,J,x1,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatFDColoringSetRecompute()"
/*@C
   MatFDColoringSetRecompute - Indicates that the next time a Jacobian preconditioner
     is needed it sholuld be recomputed. Once this is called and the new Jacobian is computed
     no additional Jacobian's will be computed (the same one will be used) until this is
     called again.

   Collective on MatFDColoring

   Input  Parameters:
.  c - the coloring context

   Level: intermediate

   Notes: The MatFDColoringSetFrequency() is ignored once this is called

.seealso: MatFDColoringCreate(), MatFDColoringSetFrequency()

.keywords: Mat, finite differences, coloring
@*/
int MatFDColoringSetRecompute(MatFDColoring c)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(c,MAT_FDCOLORING_COOKIE);
  c->usersetsrecompute = PETSC_TRUE;
  c->recompute         = PETSC_TRUE;
  PetscFunctionReturn(0);
}


