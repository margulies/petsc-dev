/* $Id: spooles.h,v 1.46 2001/08/07 03:02:47 balay Exp $ */

#include "src/mat/matimpl.h"

#if !defined(__SPOOLES_H)
#define __SPOOLES_H

EXTERN_C_BEGIN
#include "misc.h"
#include "FrontMtx.h"
#include "SymbFac.h"
#include "MPI/spoolesMPI.h" 
EXTERN_C_END

typedef struct {
  /* Followings are used for seq and MPI Spooles */
  InpMtx          *mtxA ;        /* coefficient matrix */
  ETree           *frontETree ;  /* defines numeric and symbolic factorizations */
  FrontMtx        *frontmtx ;    /* numeric L, D, U factor matrices */
  IV              *newToOldIV, *oldToNewIV ; /* permutation vectors */
  IVL             *symbfacIVL ;              /* symbolic factorization */
  int             msglvl,pivotingflag,symflag,seed;
  FILE            *msgFile ;
  SubMtxManager   *mtxmanager  ;  /* working array */
  double          cpus[10] ; 
  MatStructure    flg;
  int             *oldToNew,nz;
  double          tau,toosmall,fudge;
  int             ordering,maxdomainsize,maxzeros,maxsize,
                  patchAndGoFlag,storeids,storevalues;

  /* Followings are used for MPI Spooles */
  IV              *ownersIV,*ownedColumnsIV,*vtxmapIV;
  SolveMap        *solvemap ;
  DenseMtx        *mtxY, *mtxX;
  double          *entX;
  int             *rowindX,stats[20],rstart,firsttag,nmycol;
} Mat_Spooles;

EXTERN int MatDestroy_SeqAIJ_Spooles(Mat);
EXTERN int MatSolve_SeqAIJ_Spooles(Mat,Vec,Vec);
EXTERN int MatFactorNumeric_SeqAIJ_Spooles(Mat,Mat *); 

EXTERN int MatDestroy_MPIAIJ_Spooles(Mat);
EXTERN int MatSolve_MPIAIJ_Spooles(Mat,Vec,Vec);
EXTERN int MatFactorNumeric_MPIAIJ_Spooles(Mat,Mat *); 

#endif
