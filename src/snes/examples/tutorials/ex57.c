
static char help[] = "Bratu nonlinear PDE in 2d.\n\
We solve the  Bratu (SFI - solid fuel ignition) problem in a 2D rectangular\n\
domain, using distributed arrays (DMDAs) to partition the parallel grid.\n\
The command line options include:\n\
  -par <parameter>, where <parameter> indicates the problem's nonlinearity\n\
     problem SFI:  <parameter> = Bratu parameter (0 <= par <= 6.81)\n\n";

/*T
   Concepts: SNES^parallel Bratu example
   Concepts: DMDA^using distributed arrays;
   Concepts: IS coloirng types;
   Processors: n
T*/

/* ------------------------------------------------------------------------

    Solid Fuel Ignition (SFI) problem.  This problem is modeled by
    the partial differential equation
  
            -Laplacian u - lambda*exp(u) = 0,  0 < x,y < 1,
  
    with boundary conditions
   
             u = 0  for  x = 0, x = 1, y = 0, y = 1.
  
    A finite difference approximation with the usual 5-point stencil
    is used to discretize the boundary value problem to obtain a nonlinear 
    system of equations.

    Program usage:  mpiexec -n <procs> ex5 [-help] [all PETSc options] 
     e.g.,
     
      This example shows how geometric multigrid can be run transparently with a nonlinear solver so long
      as SNESSetDM() is provided. Example usage

      ./ex57 -pc_type mg -ksp_monitor  -snes_view -pc_mg_levels 3 -pc_mg_galerkin -da_grid_x 17 -da_grid_y 17 
             -mg_levels_ksp_monitor -snes_monitor -mg_levels_pc_type sor -pc_mg_type full

      or to run with grid sequencing on the nonlinear problem (note that you do not need to provide the number of 
         multigrid levels, it will be determined automatically based on the number of refinements done)

      ./ex57 -pc_type mg -ksp_monitor  -snes_view -pc_mg_galerkin -snes_grid_sequence 3
             -mg_levels_ksp_monitor -snes_monitor -mg_levels_pc_type sor -pc_mg_type full

  ------------------------------------------------------------------------- */

/* 
   Include "petscdmda.h" so that we can use distributed arrays (DMDAs).
   Include "petscsnes.h" so that we can use SNES solvers.  Note that this
*/
#include <petscdmda.h>
#include <petscsnes.h>

/* 
   User-defined application context - contains data needed by the 
   application-provided call-back routines, FormJacobianLocal() and
   FormFunctionLocal().
*/
typedef struct {
   DM          da;             /* distributed array data structure */
   PassiveReal param;          /* test problem parameter */
} AppCtx;

/* 
   User-defined routines
*/
extern PetscErrorCode FormInitialGuess(AppCtx*,Vec);
extern PetscErrorCode FormFunctionLocal(DMDALocalInfo*,PetscScalar**,PetscScalar**,AppCtx*);
extern PetscErrorCode FormJacobianLocal(DMDALocalInfo*,PetscScalar**,Mat,AppCtx*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  SNES                   snes;                 /* nonlinear solver */
  Vec                    x,r;                  /* solution, residual vectors */
  Mat                    J;                    /* Jacobian matrix */
  AppCtx                 user;                 /* user-defined work context */
  PetscInt               its;                  /* iterations for convergence */
  PetscErrorCode         ierr;
  PetscReal              bratu_lambda_max = 6.81,bratu_lambda_min = 0.;


  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscInitialize(&argc,&argv,(char *)0,help);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize problem parameters
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  user.param = 6.0;
  ierr = PetscOptionsGetReal(PETSC_NULL,"-par",&user.param,PETSC_NULL);CHKERRQ(ierr);
  if (user.param >= bratu_lambda_max || user.param <= bratu_lambda_min) SETERRQ3(PETSC_COMM_SELF,1,"Lambda, %g, is out of range, [%g, %g]", user.param, bratu_lambda_min, bratu_lambda_max);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create nonlinear solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = SNESCreate(PETSC_COMM_WORLD,&snes);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DMDA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMDACreate2d(PETSC_COMM_WORLD, DMDA_BOUNDARY_NONE, DMDA_BOUNDARY_NONE,DMDA_STENCIL_STAR,-4,-4,PETSC_DECIDE,PETSC_DECIDE,1,1,PETSC_NULL,PETSC_NULL,&user.da);CHKERRQ(ierr);
  ierr = DMDASetUniformCoordinates(user.da, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0);CHKERRQ(ierr);
  ierr = SNESSetDM(snes,user.da);CHKERRQ(ierr);

  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Extract global vectors from DMDA; then duplicate for remaining
     vectors that are the same types
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMCreateGlobalVector(user.da,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&r);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create matrix data structure; set Jacobian evaluation routine

     Set Jacobian matrix data structure and default Jacobian evaluation
     routine. User can override with:
     -snes_mf : matrix-free Newton-Krylov method with no preconditioning
                (unless user explicitly sets preconditioner) 
     -snes_mf_operator : form preconditioning matrix as set by the user,
                         but use matrix-free approx for Jacobian-vector
                         products within Newton-Krylov method

     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  
  ierr = SNESSetFunction(snes,r,SNESDAFormFunction,&user);CHKERRQ(ierr);
  ierr = DMGetMatrix(user.da,MATAIJ,&J);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes,J,J,SNESDAComputeJacobian,&user);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set local function evaluation routine
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMDASetLocalFunction(user.da,(DMDALocalFunction1)FormFunctionLocal);CHKERRQ(ierr);
  ierr = DMDASetLocalJacobian(user.da,(DMDALocalFunction1)FormJacobianLocal);CHKERRQ(ierr); 

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize nonlinear solver; set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);

  ierr = FormInitialGuess(&user,x);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = SNESSolve(snes,PETSC_NULL,x);CHKERRQ(ierr); 
  ierr = SNESGetIterationNumber(snes,&its);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&r);CHKERRQ(ierr);      
  ierr = SNESDestroy(&snes);CHKERRQ(ierr);
  ierr = DMDestroy(&user.da);CHKERRQ(ierr);
  ierr = PetscFinalize();

  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "FormInitialGuess"
/* 
   FormInitialGuess - Forms initial approximation.

   Input Parameters:
   user - user-defined application context
   X - vector

   Output Parameter:
   X - vector
 */
PetscErrorCode FormInitialGuess(AppCtx *user,Vec X)
{
  PetscInt       i,j,Mx,My,xs,ys,xm,ym;
  PetscErrorCode ierr;
  PetscReal      lambda,temp1,temp,hx,hy;
  PetscScalar    **x;

  PetscFunctionBegin;
  ierr = DMDAGetInfo(user->da,PETSC_IGNORE,&Mx,&My,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,
                   PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);

  lambda = user->param;
  hx     = 1.0/(PetscReal)(Mx-1);
  hy     = 1.0/(PetscReal)(My-1);
  temp1  = lambda/(lambda + 1.0);

  /*
     Get a pointer to vector data.
       - For default PETSc vectors, VecGetArray() returns a pointer to
         the data array.  Otherwise, the routine is implementation dependent.
       - You MUST call VecRestoreArray() when you no longer need access to
         the array.
  */
  ierr = DMDAVecGetArray(user->da,X,&x);CHKERRQ(ierr);

  /*
     Get local grid boundaries (for 2-dimensional DMDA):
       xs, ys   - starting grid indices (no ghost points)
       xm, ym   - widths of local grid (no ghost points)

  */
  ierr = DMDAGetCorners(user->da,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL);CHKERRQ(ierr);

  /*
     Compute initial guess over the locally owned part of the grid
  */
  for (j=ys; j<ys+ym; j++) {
    temp = (PetscReal)(PetscMin(j,My-j-1))*hy;
    for (i=xs; i<xs+xm; i++) {
      if (i == 0 || j == 0 || i == Mx-1 || j == My-1) {
        /* boundary conditions are all zero Dirichlet */
        x[j][i] = 0.0; 
      } else {
        x[j][i] = temp1*sqrt(PetscMin((PetscReal)(PetscMin(i,Mx-i-1))*hx,temp)); 
      }
    }
  }

  /*
     Restore vector
  */
  ierr = DMDAVecRestoreArray(user->da,X,&x);CHKERRQ(ierr);

  PetscFunctionReturn(0);
} 
/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "FormFunctionLocal"
/* 
   FormFunctionLocal - Evaluates nonlinear function, F(x) on local process patch


 */
PetscErrorCode FormFunctionLocal(DMDALocalInfo *info,PetscScalar **x,PetscScalar **f,AppCtx *user)
{
  PetscErrorCode ierr;
  PetscInt       i,j;
  PetscReal      lambda,hx,hy,hxdhy,hydhx,sc;
  PetscScalar    u,uxx,uyy;

  PetscFunctionBegin;

  lambda = user->param;
  hx     = 1.0/(PetscReal)(info->mx-1);
  hy     = 1.0/(PetscReal)(info->my-1);
  sc     = hx*hy*lambda;
  hxdhy  = hx/hy; 
  hydhx  = hy/hx;
  /*
     Compute function over the locally owned part of the grid
  */
  for (j=info->ys; j<info->ys+info->ym; j++) {
    for (i=info->xs; i<info->xs+info->xm; i++) {
      if (i == 0 || j == 0 || i == info->mx-1 || j == info->my-1) {
        f[j][i] = x[j][i];
      } else {
        u       = x[j][i];
        uxx     = (2.0*u - x[j][i-1] - x[j][i+1])*hydhx;
        uyy     = (2.0*u - x[j-1][i] - x[j+1][i])*hxdhy;
        f[j][i] = uxx + uyy - sc*PetscExpScalar(u);
      }
    }
  }
  ierr = PetscLogFlops(11.0*info->ym*info->xm);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

#undef __FUNCT__
#define __FUNCT__ "FormJacobianLocal"
/*
   FormJacobianLocal - Evaluates Jacobian matrix on local process patch
*/
PetscErrorCode FormJacobianLocal(DMDALocalInfo *info,PetscScalar **x,Mat jac,AppCtx *user)
{
  PetscErrorCode ierr;
  PetscInt       i,j;
  MatStencil     col[5],row;
  PetscScalar    lambda,v[5],hx,hy,hxdhy,hydhx,sc;

  PetscFunctionBegin;
  lambda = user->param;
  hx     = 1.0/(PetscReal)(info->mx-1);
  hy     = 1.0/(PetscReal)(info->my-1);
  sc     = hx*hy*lambda;
  hxdhy  = hx/hy; 
  hydhx  = hy/hx;


  /* 
     Compute entries for the locally owned part of the Jacobian.
      - Currently, all PETSc parallel matrix formats are partitioned by
        contiguous chunks of rows across the processors. 
      - Each processor needs to insert only elements that it owns
        locally (but any non-local elements will be sent to the
        appropriate processor during matrix assembly). 
      - Here, we set all entries for a particular row at once.
      - We can set matrix entries either using either
        MatSetValuesLocal() or MatSetValues(), as discussed above.
  */
  for (j=info->ys; j<info->ys+info->ym; j++) {
    for (i=info->xs; i<info->xs+info->xm; i++) {
      row.j = j; row.i = i;
      /* boundary points */
      if (i == 0 || j == 0 || i == info->mx-1 || j == info->my-1) {
        v[0] = 1.0;
        ierr = MatSetValuesStencil(jac,1,&row,1,&row,v,INSERT_VALUES);CHKERRQ(ierr);
      } else {
      /* interior grid points */
        v[0] = -hxdhy;                                           col[0].j = j - 1; col[0].i = i;
        v[1] = -hydhx;                                           col[1].j = j;     col[1].i = i-1;
        v[2] = 2.0*(hydhx + hxdhy) - sc*PetscExpScalar(x[j][i]); col[2].j = row.j; col[2].i = row.i;
        v[3] = -hydhx;                                           col[3].j = j;     col[3].i = i+1;
        v[4] = -hxdhy;                                           col[4].j = j + 1; col[4].i = i;
        ierr = MatSetValuesStencil(jac,1,&row,5,col,v,INSERT_VALUES);CHKERRQ(ierr);
      }
    }
  }

  /* 
     Assemble matrix, using the 2-step process:
       MatAssemblyBegin(), MatAssemblyEnd().
  */
  ierr = MatAssemblyBegin(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  /*
     Tell the matrix we will never add a new nonzero location to the
     matrix. If we do, it will generate an error.
  */
  ierr = MatSetOption(jac,MAT_NEW_NONZERO_LOCATION_ERR,PETSC_TRUE);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
