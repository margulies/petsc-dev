static char help[] = "Time-dependent PDE in 2d for calculating joint PDF. \n";
/*
   p_t = -x_t*p_x -y_t*p_y + f(t)*p_yy
   xmin < x < xmax, ymin < y < ymax; with zero boundary conditions
   x_t = ws*(y - 1.)  y_t = (ws/2H)*(Pm - Pmax*sin(x))
*/

#include <petscdmda.h>
#include <petscts.h>

/*
   User-defined data structures and routines
*/
typedef struct{
  PetscScalar ws;   /* Synchronous speed */
  PetscScalar H;    /* Inertia constant */
  PetscScalar Pmax; /* Maximum power output of generator */
  PetscScalar PM_min; /* Mean mechanical power input */
  PetscScalar lambda; /* correlation time */
  PetscScalar q;      /* noise strength */
  PetscScalar mux;    /* Initial average angle */
  PetscScalar sigmax; /* Standard deviation of initial angle */
  PetscScalar muy;    /* Average speed */
  PetscScalar sigmay; /* standard deviation of initial speed */
  PetscScalar rho;    /* Cross-correlation coefficient */
  PetscScalar   t0;     /* Initial time */
  PetscScalar   tmax;   /* Final time */
  PetscScalar xmin;   /* left boundary of angle */
  PetscScalar xmax;   /* right boundary of angle */
  PetscScalar ymin;   /* bottom boundary of speed */
  PetscScalar ymax;   /* top boundary of speed */
  PetscScalar dx;     /* x step size */
  PetscScalar dy;     /* y step size */
  DM          da;
}AppCtx;

PetscErrorCode Parameter_settings(AppCtx*);
PetscErrorCode ini_bou(Vec,AppCtx*);
PetscErrorCode RHSFunction(TS,PetscReal,Vec,Vec,void*);
PetscErrorCode RHSJacobian(TS,PetscReal,Vec,Mat*,Mat*,MatStructure*,void*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char **argv)
{
  PetscErrorCode ierr;
  Vec            x;  /* Solution vector */
  TS             ts;   /* Time-stepping context */
  AppCtx         user; /* Application context */
  Mat            J;
  PetscViewer    viewer;

  PetscInitialize(&argc,&argv,"petscopt_ex6", help);
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"",300,0,300,300,&viewer);CHKERRQ(ierr);

  /* Get physics and time parameters */
  ierr = Parameter_settings(&user);CHKERRQ(ierr);
  /* Create a 2D DA with dof = 1 */
  ierr = DMDACreate2d(PETSC_COMM_WORLD,DMDA_BOUNDARY_NONE,DMDA_BOUNDARY_NONE,DMDA_STENCIL_STAR,-4,-4,PETSC_DECIDE,PETSC_DECIDE,1,1,PETSC_NULL,PETSC_NULL,&user.da);CHKERRQ(ierr);
  /* Set x and y coordinates */
  ierr = DMDASetUniformCoordinates(user.da,user.xmin,user.xmax,user.ymin,user.ymax,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  /* Get global vector x from DM  */
  ierr = DMCreateGlobalVector(user.da,&x);CHKERRQ(ierr);

  ierr = ini_bou(x,&user);CHKERRQ(ierr);
  ierr = DMView(user.da,viewer);CHKERRQ(ierr);
  ierr = VecView(x,viewer);CHKERRQ(ierr);

  /* Get Jacobian matrix structure from the da */
  ierr = DMCreateMatrix(user.da,MATAIJ,&J);CHKERRQ(ierr);

  ierr = TSCreate(PETSC_COMM_WORLD,&ts);CHKERRQ(ierr);
  ierr = TSSetProblemType(ts,TS_NONLINEAR);CHKERRQ(ierr);
  ierr = TSSetRHSFunction(ts,PETSC_NULL,RHSFunction,&user);CHKERRQ(ierr);
  ierr = TSSetRHSJacobian(ts,J,J,RHSJacobian,&user);CHKERRQ(ierr);
  ierr = TSSetDuration(ts,PETSC_DEFAULT,user.tmax);CHKERRQ(ierr);
  ierr = TSSetInitialTimeStep(ts,user.t0,.005);CHKERRQ(ierr);
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);
  ierr = TSSolve(ts,x);CHKERRQ(ierr);

  ierr = DMView(user.da,viewer);CHKERRQ(ierr);
  ierr = VecView(x,viewer);CHKERRQ(ierr);
  
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  ierr = DMDestroy(&user.da);CHKERRQ(ierr);
  ierr = TSDestroy(&ts);CHKERRQ(ierr);
  PetscFinalize();
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "ini_bou"
PetscErrorCode ini_bou(Vec X,AppCtx* user)
{
  PetscErrorCode ierr;
  DM             cda;
  DMDACoor2d    **coors;
  PetscScalar   **p;
  Vec            gc;
  PetscInt       i,j;
  PetscInt       xs,ys,xm,ym,M,N;
  PetscScalar    xi,yi;
  PetscScalar      sigmax=user->sigmax,sigmay=user->sigmay;
  PetscScalar      rho=user->rho;
  PetscScalar      mux=user->mux,muy=user->muy;
  PetscMPIInt      rank;

  PetscFunctionBeginUser;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = DMDAGetInfo(user->da,PETSC_NULL,&M,&N,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL);
  user->dx = (user->xmax - user->xmin)/(M-1); user->dy = (user->ymax - user->ymin)/(N-1);
  ierr = DMGetCoordinateDM(user->da,&cda);CHKERRQ(ierr);
  ierr = DMGetCoordinates(user->da,&gc);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(cda,gc,&coors);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,X,&p);CHKERRQ(ierr);
  ierr = DMDAGetCorners(cda,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  for(i=xs; i < xs+xm; i++) {
    for(j=ys; j < ys+ym; j++) {
      xi = coors[j][i].x; yi = coors[j][i].y;
      if(i == 0 || j == 0 || i == M-1 || j == N-1) {
	p[j][i] = 0.0;
      } else {
	p[j][i] = (0.5/(PETSC_PI*sigmax*sigmay*PetscSqrtScalar(1.0-rho*rho)))*PetscExpScalar(-0.5/(1-rho*rho)*(PetscPowScalar((xi-mux)/sigmax,2) + PetscPowScalar((yi-muy)/sigmay,2) - 2*rho*(xi-mux)*(yi-muy)/(sigmax*sigmay)));
      }
    }
  }
  ierr = DMDAVecRestoreArray(cda,gc,&coors);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(user->da,X,&p);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* First advection term */
#undef __FUNCT__
#define __FUNCT__ "adv1"
PetscErrorCode adv1(PetscScalar **p,PetscScalar y,PetscInt i,PetscInt j,PetscInt M,PetscScalar *p1,AppCtx* user)
{
  /*  PetscScalar v1,v2,v3,v4,v5,s1,s2,s3; */
  PetscFunctionBegin;
  /*  if(i > 2 && i < M-2) {
    v1 = user->ws*(y-1.0)*(p[j][i-2] - p[j][i-3])/user->dx;
    v2 = user->ws*(y-1.0)*(p[j][i-1] - p[j][i-2])/user->dx;
    v3 = user->ws*(y-1.0)*(p[j][i] - p[j][i-1])/user->dx;
    v4 = user->ws*(y-1.0)*(p[j][i+1] - p[j][i])/user->dx;
    v5 = user->ws*(y-1.0)*(p[j][i+1] - p[j][i+2])/user->dx;

    s1 = v1/3.0 - (7.0/6.0)*v2 + (11.0/6.0)*v3;
    s2 =-v2/6.0 + (5.0/6.0)*v3 + (1.0/3.0)*v4;
    s3 = v3/3.0 + (5.0/6.0)*v4 - (1.0/6.0)*v5;

    *p1 = 0.1*s1 + 0.6*s2 + 0.3*s3;
    } else *p1 = 0.0; */
  *p1 = user->ws*(y-1.0)*(p[j][i+1] - p[j][i-1])/(2*user->dx); 
  PetscFunctionReturn(0);
}

/* Second advection term */
#undef __FUNCT__
#define __FUNCT__ "adv2"
PetscErrorCode adv2(PetscScalar **p,PetscScalar x,PetscInt i,PetscInt j,PetscInt N,PetscScalar *p2,AppCtx* user)
{
  /*  PetscScalar v1,v2,v3,v4,v5,s1,s2,s3; */
  PetscFunctionBegin;
  /*  if(j > 2 && j < N-2) {
    v1 = (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x))*(p[j-2][i] - p[j-3][i])/user->dy;
    v2 = (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x))*(p[j-1][i] - p[j-2][i])/user->dy;
    v3 = (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x))*(p[j][i] - p[j-1][i])/user->dy;
    v4 = (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x))*(p[j+1][i] - p[j][i])/user->dy;
    v5 = (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x))*(p[j+2][i] - p[j+1][i])/user->dy;

    s1 = v1/3.0 - (7.0/6.0)*v2 + (11.0/6.0)*v3;
    s2 =-v2/6.0 + (5.0/6.0)*v3 + (1.0/3.0)*v4;
    s3 = v3/3.0 + (5.0/6.0)*v4 - (1.0/6.0)*v5;

    *p2 = 0.1*s1 + 0.6*s2 + 0.3*s3;
    } else *p2 = 0.0; */
  *p2 =  (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x))*(p[j+1][i] - p[j-1][i])/(2*user->dy);
  PetscFunctionReturn(0);
}

/* Diffusion term */
#undef __FUNCT__
#define __FUNCT__ "diffuse"
PetscErrorCode diffuse(PetscScalar **p,PetscInt i,PetscInt j,PetscReal t,PetscScalar *p_diff,AppCtx* user)
{
  PetscScalar disper_coe;
  PetscFunctionBeginUser;

  disper_coe = PetscPowScalar((user->lambda*user->ws)/(2*user->H),2)*user->q*(1.0-PetscExpScalar(-t/user->lambda));
  *p_diff = disper_coe*((p[j-1][i] - 2*p[j][i] + p[j+1][i])/(user->dy*user->dy));
  PetscFunctionReturn(0);
}
    
#undef __FUNCT__
#define __FUNCT__ "RHSFunction"
PetscErrorCode RHSFunction(TS ts,PetscReal t,Vec X,Vec F,void* ctx)
{
  PetscErrorCode ierr;
  AppCtx         *user=(AppCtx*)ctx;
  DM             cda;
  DMDACoor2d     **coors;
  PetscScalar    **p,**f;
  PetscInt       i,j;
  PetscInt       xs,ys,xm,ym,M,N;
  Vec            localX,gc;
  PetscScalar    p_adv1,p_adv2,p_diff;

  PetscFunctionBeginUser;
  ierr = DMDAGetInfo(user->da,PETSC_NULL,&M,&N,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL);
  ierr = DMGetCoordinateDM(user->da,&cda);CHKERRQ(ierr);
  ierr = DMDAGetCorners(cda,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);

  ierr = DMGetLocalVector(user->da,&localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalBegin(user->da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(user->da,X,INSERT_VALUES,localX);CHKERRQ(ierr);

  ierr = DMGetCoordinatesLocal(user->da,&gc);CHKERRQ(ierr);

  ierr = DMDAVecGetArray(cda,gc,&coors);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,localX,&p);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,F,&f);CHKERRQ(ierr);

  for(i=xs; i < xs+xm; i++) {
    for(j=ys; j < ys+ym; j++) {
      if(i == 0 || j == 0 || i == M-1 || j == N-1) {
	f[j][i] = p[j][i];
      } else {	
	ierr = adv1(p,coors[j][i].y,i,j,M,&p_adv1,user);CHKERRQ(ierr);
	ierr = adv2(p,coors[j][i].x,i,j,N,&p_adv2,user);CHKERRQ(ierr);
	ierr = diffuse(p,i,j,t,&p_diff,user);CHKERRQ(ierr);
	if(p_adv1!=0 || p_adv2!=0 || p_diff!=0.0) {
	  p_diff = p_diff;
	}
	f[j][i] = -p_adv1 - p_adv2 + p_diff;
      }
    }
  }
  ierr = DMDAVecRestoreArray(user->da,localX,&p);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(user->da,&localX);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(user->da,F,&f);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(cda,gc,&coors);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "RHSJacobian"
PetscErrorCode RHSJacobian(TS ts,PetscReal t,Vec X,Mat *J,Mat *Jpre,MatStructure* flg,void* ctx)
{
  PetscErrorCode ierr;
  AppCtx         *user=(AppCtx*)ctx;
  DM             cda;
  DMDACoor2d     **coors;
  PetscInt       i,j;
  PetscInt       xs,ys,xm,ym,M,N;
  Vec            gc;
  PetscScalar    val[5],xi,yi;
  MatStencil     row,col[5];
  PetscScalar    c1,c2,c3;

  PetscFunctionBeginUser;
  *flg = SAME_NONZERO_PATTERN;
  ierr = DMDAGetInfo(user->da,PETSC_NULL,&M,&N,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL);
  ierr = DMGetCoordinateDM(user->da,&cda);CHKERRQ(ierr);
  ierr = DMDAGetCorners(cda,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);

  ierr = DMGetCoordinatesLocal(user->da,&gc);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(cda,gc,&coors);CHKERRQ(ierr);
  for(i=xs; i < xs+xm; i++) {
    for(j=ys; j < ys+ym; j++) {
      xi = coors[j][i].x; yi = coors[j][i].y;
      PetscInt nc = 0;
      row.i = i; row.j = j;
      if(i == 0 || j == 0 || i == M-1 || j == N-1) {
	col[nc].i = i; col[nc].j = j; val[nc++] = 1.0;
      } else {
	c1 = user->ws*(yi-1.0)/(2*user->dx);
	c2 = (user->ws/(2.0*user->H))*(user->PM_min - user->Pmax*sin(xi))/(2*user->dy);
	c3 = (PetscPowScalar((user->lambda*user->ws)/(2*user->H),2)*user->q*(1.0-PetscExpScalar(-t/user->lambda)))/(user->dy*user->dy);
	col[nc].i = i-1; col[nc].j = j;   val[nc++] = c1;
        col[nc].i = i+1; col[nc].j = j;   val[nc++] = -c1;
	col[nc].i = i;   col[nc].j = j-1; val[nc++] = c2 + c3;
	col[nc].i = i;   col[nc].j = j+1; val[nc++] = -c2 + c3;
	col[nc].i = i;   col[nc].j = j;   val[nc++] = -2*c3;
      }
      ierr = MatSetValuesStencil(*Jpre,1,&row,nc,col,val,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = DMDAVecRestoreArray(cda,gc,&coors);CHKERRQ(ierr);

  ierr =  MatAssemblyBegin(*Jpre,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*Jpre,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (*J != *Jpre) {
    ierr = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}



#undef __FUNCT__
#define __FUNCT__ "Parameter_settings"
PetscErrorCode Parameter_settings(AppCtx* user)
{
  PetscErrorCode ierr;
  PetscBool      flg;

  PetscFunctionBeginUser;

  /* Set default parameters */
  user->ws = 1.0;     
  user->H = 5.0;      user->Pmax = 2.1;
  user->PM_min = 1.0; user->lambda = 0.1;
  user->q = 1.0;      user->mux = asin(user->PM_min/user->Pmax);
  user->sigmax = 0.1; user->muy = user->ws;
  user->sigmay = 0.1; user->rho = 0.0;
  user->t0 = 0.0;     user->tmax = 2.0;
  user->xmin = -1.0; user->xmax = 10.0;
  user->ymin = -1.0; user->ymax = 10.0;
  
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-ws",&user->ws,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-Inertia",&user->H,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-Pmax",&user->Pmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-PM_min",&user->PM_min,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-lambda",&user->lambda,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-q",&user->q,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-mux",&user->mux,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-sigmax",&user->sigmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-muy",&user->muy,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-sigmay",&user->sigmay,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-rho",&user->rho,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-t0",&user->t0,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-tmax",&user->tmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-xmin",&user->xmin,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-xmax",&user->xmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-ymin",&user->ymin,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-ymax",&user->ymax,&flg);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
