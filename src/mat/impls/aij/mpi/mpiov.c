/*$Id: mpiov.c,v 1.26.1.76.2.22 2001/09/07 20:09:38 bsmith Exp $*/
/*
   Routines to compute overlapping regions of a parallel MPI matrix
  and to find submatrices that were shared across processors.
*/
#include "src/mat/impls/aij/mpi/mpiaij.h"
#include "petscbt.h"

static int MatIncreaseOverlap_MPIAIJ_Once(Mat,int,IS *);
static int MatIncreaseOverlap_MPIAIJ_Local(Mat,int,char **,int*,int**);
static int MatIncreaseOverlap_MPIAIJ_Receive(Mat,int,int **,int**,int*);
EXTERN int MatGetRow_MPIAIJ(Mat,int,int*,int**,PetscScalar**);
EXTERN int MatRestoreRow_MPIAIJ(Mat,int,int*,int**,PetscScalar**);

#undef __FUNCT__  
#define __FUNCT__ "MatIncreaseOverlap_MPIAIJ"
int MatIncreaseOverlap_MPIAIJ(Mat C,int imax,IS is[],int ov)
{
  int i,ierr;

  PetscFunctionBegin;
  if (ov < 0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Negative overlap specified");
  for (i=0; i<ov; ++i) {
    ierr = MatIncreaseOverlap_MPIAIJ_Once(C,imax,is);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*
  Sample message format:
  If a processor A wants processor B to process some elements corresponding
  to index sets is[1],is[5]
  mesg [0] = 2   (no of index sets in the mesg)
  -----------  
  mesg [1] = 1 => is[1]
  mesg [2] = sizeof(is[1]);
  -----------  
  mesg [3] = 5  => is[5]
  mesg [4] = sizeof(is[5]);
  -----------
  mesg [5] 
  mesg [n]  datas[1]
  -----------  
  mesg[n+1]
  mesg[m]  data(is[5])
  -----------  
  
  Notes:
  nrqs - no of requests sent (or to be sent out)
  nrqr - no of requests recieved (which have to be or which have been processed
*/
#undef __FUNCT__  
#define __FUNCT__ "MatIncreaseOverlap_MPIAIJ_Once"
static int MatIncreaseOverlap_MPIAIJ_Once(Mat C,int imax,IS is[])
{
  Mat_MPIAIJ  *c = (Mat_MPIAIJ*)C->data;
  int         **idx,*n,*w1,*w2,*w3,*w4,*rtable,**data,len,*idx_i;
  int         size,rank,m,i,j,k,ierr,**rbuf,row,proc,nrqs,msz,**outdat,**ptr;
  int         *ctr,*pa,*tmp,nrqr,*isz,*isz1,**xdata,**rbuf2;
  int         *onodes1,*olengths1,tag1,tag2,*onodes2,*olengths2;
  PetscBT     *table;
  MPI_Comm    comm;
  MPI_Request *s_waits1,*r_waits1,*s_waits2,*r_waits2;
  MPI_Status  *s_status,*recv_status;

  PetscFunctionBegin;
  comm   = C->comm;
  size   = c->size;
  rank   = c->rank;
  m      = C->M;

  ierr = PetscObjectGetNewTag((PetscObject)C,&tag1);CHKERRQ(ierr);
  ierr = PetscObjectGetNewTag((PetscObject)C,&tag2);CHKERRQ(ierr);
 
  len    = (imax+1)*sizeof(int*)+ (imax + m)*sizeof(int);
  ierr   = PetscMalloc(len,&idx);CHKERRQ(ierr);
  n      = (int*)(idx + imax);
  rtable = n + imax;
   
  for (i=0; i<imax; i++) {
    ierr = ISGetIndices(is[i],&idx[i]);CHKERRQ(ierr);
    ierr = ISGetLocalSize(is[i],&n[i]);CHKERRQ(ierr);
  }
  
  /* Create hash table for the mapping :row -> proc*/
  for (i=0,j=0; i<size; i++) {
    len = c->rowners[i+1];  
    for (; j<len; j++) {
      rtable[j] = i;
    }
  }

  /* evaluate communication - mesg to who,length of mesg, and buffer space
     required. Based on this, buffers are allocated, and data copied into them*/
  ierr = PetscMalloc(size*4*sizeof(int),&w1);CHKERRQ(ierr);/*  mesg size */
  w2   = w1 + size;       /* if w2[i] marked, then a message to proc i*/
  w3   = w2 + size;       /* no of IS that needs to be sent to proc i */
  w4   = w3 + size;       /* temp work space used in determining w1, w2, w3 */
  ierr = PetscMemzero(w1,size*3*sizeof(int));CHKERRQ(ierr); /* initialise work vector*/
  for (i=0; i<imax; i++) { 
    ierr  = PetscMemzero(w4,size*sizeof(int));CHKERRQ(ierr); /* initialise work vector*/
    idx_i = idx[i];
    len   = n[i];
    for (j=0; j<len; j++) {
      row  = idx_i[j];
      if (row < 0) {
        SETERRQ(1,"Index set cannot have negative entries");
      }
      proc = rtable[row];
      w4[proc]++;
    }
    for (j=0; j<size; j++){ 
      if (w4[j]) { w1[j] += w4[j]; w3[j]++;} 
    }
  }

  nrqs     = 0;              /* no of outgoing messages */
  msz      = 0;              /* total mesg length (for all proc */
  w1[rank] = 0;              /* no mesg sent to intself */
  w3[rank] = 0;
  for (i=0; i<size; i++) {
    if (w1[i])  {w2[i] = 1; nrqs++;} /* there exists a message to proc i */
  }
  /* pa - is list of processors to communicate with */
  ierr = PetscMalloc((nrqs+1)*sizeof(int),&pa);CHKERRQ(ierr);
  for (i=0,j=0; i<size; i++) {
    if (w1[i]) {pa[j] = i; j++;}
  } 

  /* Each message would have a header = 1 + 2*(no of IS) + data */
  for (i=0; i<nrqs; i++) {
    j      = pa[i];
    w1[j] += w2[j] + 2*w3[j];   
    msz   += w1[j];  
  }

  /* Determine the number of messages to expect, their lengths, from from-ids */
  ierr = PetscGatherNumberOfMessages(comm,w2,w1,&nrqr);CHKERRQ(ierr);
  ierr = PetscGatherMessageLengths(comm,nrqs,nrqr,w1,&onodes1,&olengths1);CHKERRQ(ierr);

  /* Now post the Irecvs corresponding to these messages */
  ierr = PetscPostIrecvInt(comm,tag1,nrqr,onodes1,olengths1,&rbuf,&r_waits1);CHKERRQ(ierr);

  /* Allocate Memory for outgoing messages */
  len  = 2*size*sizeof(int*) + (size+msz)*sizeof(int);
  ierr = PetscMalloc(len,&outdat);CHKERRQ(ierr);
  ptr  = outdat + size;     /* Pointers to the data in outgoing buffers */
  ierr = PetscMemzero(outdat,2*size*sizeof(int*));CHKERRQ(ierr);
  tmp  = (int*)(outdat + 2*size);
  ctr  = tmp + msz;

  {
    int *iptr = tmp,ict  = 0;
    for (i=0; i<nrqs; i++) {
      j         = pa[i];
      iptr     +=  ict;
      outdat[j] = iptr;
      ict       = w1[j];
    }
  }

  /* Form the outgoing messages */
  /*plug in the headers*/
  for (i=0; i<nrqs; i++) {
    j            = pa[i];
    outdat[j][0] = 0;
    ierr         = PetscMemzero(outdat[j]+1,2*w3[j]*sizeof(int));CHKERRQ(ierr);
    ptr[j]       = outdat[j] + 2*w3[j] + 1;
  }
 
  /* Memory for doing local proc's work*/
  { 
    int  *d_p;
    char *t_p;

    len   = (imax)*(sizeof(PetscBT) + sizeof(int*)+ sizeof(int)) + 
      (m)*imax*sizeof(int)  + (m/PETSC_BITS_PER_BYTE+1)*imax*sizeof(char) + 1;
    ierr  = PetscMalloc(len,&table);CHKERRQ(ierr);
    ierr  = PetscMemzero(table,len);CHKERRQ(ierr);
    data  = (int **)(table + imax);
    isz   = (int  *)(data  + imax);
    d_p   = (int  *)(isz   + imax);
    t_p   = (char *)(d_p   + m*imax);
    for (i=0; i<imax; i++) {
      table[i] = t_p + (m/PETSC_BITS_PER_BYTE+1)*i;
      data[i]  = d_p + (m)*i;
    }
  }

  /* Parse the IS and update local tables and the outgoing buf with the data*/
  {
    int     n_i,*data_i,isz_i,*outdat_j,ctr_j;
    PetscBT table_i;

    for (i=0; i<imax; i++) {
      ierr    = PetscMemzero(ctr,size*sizeof(int));CHKERRQ(ierr);
      n_i     = n[i];
      table_i = table[i];
      idx_i   = idx[i];
      data_i  = data[i];
      isz_i   = isz[i];
      for (j=0;  j<n_i; j++) {  /* parse the indices of each IS */
        row  = idx_i[j];
        proc = rtable[row];
        if (proc != rank) { /* copy to the outgoing buffer */
          ctr[proc]++;
          *ptr[proc] = row;
          ptr[proc]++;
        } else { /* Update the local table */
          if (!PetscBTLookupSet(table_i,row)) { data_i[isz_i++] = row;}
        }
      }
      /* Update the headers for the current IS */
      for (j=0; j<size; j++) { /* Can Optimise this loop by using pa[] */
        if ((ctr_j = ctr[j])) {
          outdat_j        = outdat[j];
          k               = ++outdat_j[0];
          outdat_j[2*k]   = ctr_j;
          outdat_j[2*k-1] = i;
        }
      }
      isz[i] = isz_i;
    }
  }
  


  /*  Now  post the sends */
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Request),&s_waits1);CHKERRQ(ierr);
  for (i=0; i<nrqs; ++i) {
    j    = pa[i];
    ierr = MPI_Isend(outdat[j],w1[j],MPI_INT,j,tag1,comm,s_waits1+i);CHKERRQ(ierr);
  }
    
  /* No longer need the original indices*/
  for (i=0; i<imax; ++i) {
    ierr = ISRestoreIndices(is[i],idx+i);CHKERRQ(ierr);
  }
  ierr = PetscFree(idx);CHKERRQ(ierr);

  for (i=0; i<imax; ++i) {
    ierr = ISDestroy(is[i]);CHKERRQ(ierr);
  }
  
  /* Do Local work*/
  ierr = MatIncreaseOverlap_MPIAIJ_Local(C,imax,table,isz,data);CHKERRQ(ierr);

  /* Receive messages*/
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Status),&recv_status);CHKERRQ(ierr);
  ierr = MPI_Waitall(nrqr,r_waits1,recv_status);CHKERRQ(ierr);
  
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Status),&s_status);CHKERRQ(ierr);
  ierr = MPI_Waitall(nrqs,s_waits1,s_status);CHKERRQ(ierr);

  /* Phase 1 sends are complete - deallocate buffers */
  ierr = PetscFree(outdat);CHKERRQ(ierr);
  ierr = PetscFree(w1);CHKERRQ(ierr);

  ierr = PetscMalloc((nrqr+1)*sizeof(int *),&xdata);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqr+1)*sizeof(int),&isz1);CHKERRQ(ierr);
  ierr = MatIncreaseOverlap_MPIAIJ_Receive(C,nrqr,rbuf,xdata,isz1);CHKERRQ(ierr);
  ierr = PetscFree(rbuf);CHKERRQ(ierr);

 
 /* Send the data back*/
  /* Do a global reduction to know the buffer space req for incoming messages*/
  {
    int *rw1;
    
    ierr = PetscMalloc(size*sizeof(int),&rw1);CHKERRQ(ierr);
    ierr = PetscMemzero(rw1,size*sizeof(int));CHKERRQ(ierr);

    for (i=0; i<nrqr; ++i) {
      proc      = recv_status[i].MPI_SOURCE;
      if (proc != onodes1[i]) SETERRQ(1,"MPI_SOURCE mismatch");
      rw1[proc] = isz1[i];
    }
    ierr = PetscFree(onodes1);CHKERRQ(ierr);
    ierr = PetscFree(olengths1);CHKERRQ(ierr);

    /* Determine the number of messages to expect, their lengths, from from-ids */
    ierr = PetscGatherMessageLengths(comm,nrqr,nrqs,rw1,&onodes2,&olengths2);CHKERRQ(ierr);
    PetscFree(rw1);
  }
  /* Now post the Irecvs corresponding to these messages */
  ierr = PetscPostIrecvInt(comm,tag2,nrqs,onodes2,olengths2,&rbuf2,&r_waits2);CHKERRQ(ierr);

  /*  Now  post the sends */
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Request),&s_waits2);CHKERRQ(ierr);
  for (i=0; i<nrqr; ++i) {
    j    = recv_status[i].MPI_SOURCE;
    ierr = MPI_Isend(xdata[i],isz1[i],MPI_INT,j,tag2,comm,s_waits2+i);CHKERRQ(ierr);
  }

  /* receive work done on other processors*/
  {
    int         idex,is_no,ct1,max,*rbuf2_i,isz_i,*data_i,jmax;
    PetscBT     table_i;
    MPI_Status  *status2;
     
    ierr = PetscMalloc((PetscMax(nrqr,nrqs)+1)*sizeof(MPI_Status),&status2);CHKERRQ(ierr);
    for (i=0; i<nrqs; ++i) {
      ierr = MPI_Waitany(nrqs,r_waits2,&idex,status2+i);CHKERRQ(ierr);
      /* Process the message*/
      rbuf2_i = rbuf2[idex];
      ct1     = 2*rbuf2_i[0]+1;
      jmax    = rbuf2[idex][0];
      for (j=1; j<=jmax; j++) {
        max     = rbuf2_i[2*j];
        is_no   = rbuf2_i[2*j-1];
        isz_i   = isz[is_no];
        data_i  = data[is_no];
        table_i = table[is_no];
        for (k=0; k<max; k++,ct1++) {
          row = rbuf2_i[ct1];
          if (!PetscBTLookupSet(table_i,row)) { data_i[isz_i++] = row;}   
        }
        isz[is_no] = isz_i;
      }
    }

    ierr = MPI_Waitall(nrqr,s_waits2,status2);CHKERRQ(ierr);
    ierr = PetscFree(status2);CHKERRQ(ierr);
  }
  
  for (i=0; i<imax; ++i) {
    ierr = ISCreateGeneral(PETSC_COMM_SELF,isz[i],data[i],is+i);CHKERRQ(ierr);
  }
  
  ierr = PetscFree(onodes2);CHKERRQ(ierr);
  ierr = PetscFree(olengths2);CHKERRQ(ierr);

  ierr = PetscFree(pa);CHKERRQ(ierr);
  ierr = PetscFree(rbuf2);CHKERRQ(ierr);
  ierr = PetscFree(s_waits1);CHKERRQ(ierr);
  ierr = PetscFree(r_waits1);CHKERRQ(ierr);
  ierr = PetscFree(s_waits2);CHKERRQ(ierr);
  ierr = PetscFree(r_waits2);CHKERRQ(ierr);
  ierr = PetscFree(table);CHKERRQ(ierr);
  ierr = PetscFree(s_status);CHKERRQ(ierr);
  ierr = PetscFree(recv_status);CHKERRQ(ierr);
  ierr = PetscFree(xdata[0]);CHKERRQ(ierr);
  ierr = PetscFree(xdata);CHKERRQ(ierr);
  ierr = PetscFree(isz1);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatIncreaseOverlap_MPIAIJ_Local"
/*  
   MatIncreaseOverlap_MPIAIJ_Local - Called by MatincreaseOverlap, to do 
       the work on the local processor.

     Inputs:
      C      - MAT_MPIAIJ;
      imax - total no of index sets processed at a time;
      table  - an array of char - size = m bits.
      
     Output:
      isz    - array containing the count of the solution elements correspondign
               to each index set;
      data   - pointer to the solutions
*/
static int MatIncreaseOverlap_MPIAIJ_Local(Mat C,int imax,PetscBT *table,int *isz,int **data)
{
  Mat_MPIAIJ *c = (Mat_MPIAIJ*)C->data;
  Mat        A = c->A,B = c->B;
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data,*b = (Mat_SeqAIJ*)B->data;
  int        start,end,val,max,rstart,cstart,*ai,*aj;
  int        *bi,*bj,*garray,i,j,k,row,*data_i,isz_i;
  PetscBT    table_i;

  PetscFunctionBegin;
  rstart = c->rstart;
  cstart = c->cstart;
  ai     = a->i;
  aj     = a->j;
  bi     = b->i;
  bj     = b->j;
  garray = c->garray;

  
  for (i=0; i<imax; i++) {
    data_i  = data[i];
    table_i = table[i];
    isz_i   = isz[i];
    for (j=0,max=isz[i]; j<max; j++) {
      row   = data_i[j] - rstart;
      start = ai[row];
      end   = ai[row+1];
      for (k=start; k<end; k++) { /* Amat */
        val = aj[k] + cstart;
        if (!PetscBTLookupSet(table_i,val)) { data_i[isz_i++] = val;}  
      }
      start = bi[row];
      end   = bi[row+1];
      for (k=start; k<end; k++) { /* Bmat */
        val = garray[bj[k]]; 
        if (!PetscBTLookupSet(table_i,val)) { data_i[isz_i++] = val;}  
      } 
    }
    isz[i] = isz_i;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatIncreaseOverlap_MPIAIJ_Receive"
/*     
      MatIncreaseOverlap_MPIAIJ_Receive - Process the recieved messages,
         and return the output

         Input:
           C    - the matrix
           nrqr - no of messages being processed.
           rbuf - an array of pointers to the recieved requests
           
         Output:
           xdata - array of messages to be sent back
           isz1  - size of each message

  For better efficiency perhaps we should malloc seperately each xdata[i],
then if a remalloc is required we need only copy the data for that one row
rather then all previous rows as it is now where a single large chunck of 
memory is used.

*/
static int MatIncreaseOverlap_MPIAIJ_Receive(Mat C,int nrqr,int **rbuf,int **xdata,int * isz1)
{
  Mat_MPIAIJ  *c = (Mat_MPIAIJ*)C->data;
  Mat         A = c->A,B = c->B;
  Mat_SeqAIJ  *a = (Mat_SeqAIJ*)A->data,*b = (Mat_SeqAIJ*)B->data;
  int         rstart,cstart,*ai,*aj,*bi,*bj,*garray,i,j,k;
  int         row,total_sz,ct,ct1,ct2,ct3,mem_estimate,oct2,l,start,end;
  int         val,max1,max2,rank,m,no_malloc =0,*tmp,new_estimate,ctr;
  int         *rbuf_i,kmax,rbuf_0,ierr;
  PetscBT     xtable;

  PetscFunctionBegin;
  rank   = c->rank;
  m      = C->M;
  rstart = c->rstart;
  cstart = c->cstart;
  ai     = a->i;
  aj     = a->j;
  bi     = b->i;
  bj     = b->j;
  garray = c->garray;
  
  
  for (i=0,ct=0,total_sz=0; i<nrqr; ++i) {
    rbuf_i  =  rbuf[i]; 
    rbuf_0  =  rbuf_i[0];
    ct     += rbuf_0;
    for (j=1; j<=rbuf_0; j++) { total_sz += rbuf_i[2*j]; }
  }
  
  if (C->m) max1 = ct*(a->nz + b->nz)/C->m;
  else      max1 = 1;
  mem_estimate = 3*((total_sz > max1 ? total_sz : max1)+1);
  ierr         = PetscMalloc(mem_estimate*sizeof(int),&xdata[0]);CHKERRQ(ierr);
  ++no_malloc;
  ierr         = PetscBTCreate(m,xtable);CHKERRQ(ierr);
  ierr         = PetscMemzero(isz1,nrqr*sizeof(int));CHKERRQ(ierr);
  
  ct3 = 0;
  for (i=0; i<nrqr; i++) { /* for easch mesg from proc i */
    rbuf_i =  rbuf[i]; 
    rbuf_0 =  rbuf_i[0];
    ct1    =  2*rbuf_0+1;
    ct2    =  ct1;
    ct3    += ct1;
    for (j=1; j<=rbuf_0; j++) { /* for each IS from proc i*/
      ierr = PetscBTMemzero(m,xtable);CHKERRQ(ierr);
      oct2 = ct2;
      kmax = rbuf_i[2*j];
      for (k=0; k<kmax; k++,ct1++) { 
        row = rbuf_i[ct1];
        if (!PetscBTLookupSet(xtable,row)) { 
          if (!(ct3 < mem_estimate)) {
            new_estimate = (int)(1.5*mem_estimate)+1;
            ierr         = PetscMalloc(new_estimate*sizeof(int),&tmp);CHKERRQ(ierr);
            ierr         = PetscMemcpy(tmp,xdata[0],mem_estimate*sizeof(int));CHKERRQ(ierr);
            ierr         = PetscFree(xdata[0]);CHKERRQ(ierr);
            xdata[0]     = tmp;
            mem_estimate = new_estimate; ++no_malloc;
            for (ctr=1; ctr<=i; ctr++) { xdata[ctr] = xdata[ctr-1] + isz1[ctr-1];}
          }
          xdata[i][ct2++] = row;
          ct3++;
        }
      }
      for (k=oct2,max2=ct2; k<max2; k++) {
        row   = xdata[i][k] - rstart;
        start = ai[row];
        end   = ai[row+1];
        for (l=start; l<end; l++) {
          val = aj[l] + cstart;
          if (!PetscBTLookupSet(xtable,val)) {
            if (!(ct3 < mem_estimate)) {
              new_estimate = (int)(1.5*mem_estimate)+1;
              ierr         = PetscMalloc(new_estimate*sizeof(int),&tmp);CHKERRQ(ierr);
              ierr         = PetscMemcpy(tmp,xdata[0],mem_estimate*sizeof(int));CHKERRQ(ierr);
              ierr         = PetscFree(xdata[0]);CHKERRQ(ierr);
              xdata[0]     = tmp;
              mem_estimate = new_estimate; ++no_malloc;
              for (ctr=1; ctr<=i; ctr++) { xdata[ctr] = xdata[ctr-1] + isz1[ctr-1];}
            }
            xdata[i][ct2++] = val;
            ct3++;
          }
        }
        start = bi[row];
        end   = bi[row+1];
        for (l=start; l<end; l++) {
          val = garray[bj[l]];
          if (!PetscBTLookupSet(xtable,val)) { 
            if (!(ct3 < mem_estimate)) { 
              new_estimate = (int)(1.5*mem_estimate)+1;
              ierr         = PetscMalloc(new_estimate*sizeof(int),&tmp);CHKERRQ(ierr);
              ierr         = PetscMemcpy(tmp,xdata[0],mem_estimate*sizeof(int));CHKERRQ(ierr);
              ierr = PetscFree(xdata[0]);CHKERRQ(ierr);
              xdata[0]     = tmp;
              mem_estimate = new_estimate; ++no_malloc;
              for (ctr =1; ctr <=i; ctr++) { xdata[ctr] = xdata[ctr-1] + isz1[ctr-1];}
            }
            xdata[i][ct2++] = val;
            ct3++;
          }  
        } 
      }
      /* Update the header*/
      xdata[i][2*j]   = ct2 - oct2; /* Undo the vector isz1 and use only a var*/
      xdata[i][2*j-1] = rbuf_i[2*j-1];
    }
    xdata[i][0] = rbuf_0;
    xdata[i+1]  = xdata[i] + ct2;
    isz1[i]     = ct2; /* size of each message */
  }
  ierr = PetscBTDestroy(xtable);CHKERRQ(ierr);
  PetscLogInfo(0,"MatIncreaseOverlap_MPIAIJ:[%d] Allocated %d bytes, required %d bytes, no of mallocs = %d\n",rank,mem_estimate, ct3,no_malloc);    
  PetscFunctionReturn(0);
}  
/* -------------------------------------------------------------------------*/
EXTERN int MatGetSubMatrices_MPIAIJ_Local(Mat,int,const IS[],const IS[],MatReuse,Mat*);
EXTERN int MatAssemblyEnd_SeqAIJ(Mat,MatAssemblyType);
/*
    Every processor gets the entire matrix
*/
#undef __FUNCT__  
#define __FUNCT__ "MatGetSubMatrix_MPIAIJ_All" 
int MatGetSubMatrix_MPIAIJ_All(Mat A,MatReuse scall,Mat *Bin[])
{
  Mat          B;
  Mat_MPIAIJ   *a = (Mat_MPIAIJ *)A->data;
  Mat_SeqAIJ   *b,*ad = (Mat_SeqAIJ*)a->A->data,*bd = (Mat_SeqAIJ*)a->B->data;
  int          ierr,sendcount,*recvcounts = 0,*displs = 0,size,i,*rstarts = a->rowners,rank,n,cnt,j;
  int          m,*b_sendj,*garray = a->garray,*lens,*jsendbuf,*a_jsendbuf,*b_jsendbuf;
  PetscScalar  *sendbuf,*recvbuf,*a_sendbuf,*b_sendbuf;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(A->comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(A->comm,&rank);CHKERRQ(ierr);

  if (scall == MAT_INITIAL_MATRIX) {
    /* ----------------------------------------------------------------
         Tell every processor the number of nonzeros per row
    */
    ierr = PetscMalloc(A->M*sizeof(int),&lens);CHKERRQ(ierr);
    for (i=a->rstart; i<a->rend; i++) {
      lens[i] = ad->i[i-a->rstart+1] - ad->i[i-a->rstart] + bd->i[i-a->rstart+1] - bd->i[i-a->rstart];
    }
    sendcount = a->rend - a->rstart;
    ierr = PetscMalloc(2*size*sizeof(int),&recvcounts);CHKERRQ(ierr);
    displs     = recvcounts + size;
    for (i=0; i<size; i++) {
      recvcounts[i] = a->rowners[i+1] - a->rowners[i];
      displs[i]     = a->rowners[i];
    }
    ierr  = MPI_Allgatherv(lens+a->rstart,sendcount,MPI_INT,lens,recvcounts,displs,MPI_INT,A->comm);CHKERRQ(ierr);

    /* ---------------------------------------------------------------
         Create the sequential matrix of the same type as the local block diagonal
    */
    ierr  = MatCreate(PETSC_COMM_SELF,A->M,A->N,PETSC_DETERMINE,PETSC_DETERMINE,&B);CHKERRQ(ierr);
    ierr  = MatSetType(B,a->A->type_name);CHKERRQ(ierr);
    ierr  = MatSeqAIJSetPreallocation(B,0,lens);CHKERRQ(ierr);
    ierr  = PetscMalloc(sizeof(Mat),Bin);CHKERRQ(ierr);
    **Bin = B;
    b = (Mat_SeqAIJ *)B->data;

    /*--------------------------------------------------------------------
       Copy my part of matrix column indices over
    */
    sendcount  = ad->nz + bd->nz;
    jsendbuf   = b->j + b->i[rstarts[rank]];
    a_jsendbuf = ad->j;
    b_jsendbuf = bd->j;
    n          = a->rend - a->rstart;
    cnt        = 0;
    for (i=0; i<n; i++) {

      /* put in lower diagonal portion */
      m = bd->i[i+1] - bd->i[i];
      while (m > 0) {
        /* is it above diagonal (in bd (compressed) numbering) */
        if (garray[*b_jsendbuf] > a->rstart + i) break;
        jsendbuf[cnt++] = garray[*b_jsendbuf++];
        m--;
      }

      /* put in diagonal portion */
      for (j=ad->i[i]; j<ad->i[i+1]; j++) {
        jsendbuf[cnt++] = a->rstart + *a_jsendbuf++;
      }

      /* put in upper diagonal portion */
      while (m-- > 0) {
        jsendbuf[cnt++] = garray[*b_jsendbuf++];
      }
    }
    if (cnt != sendcount) SETERRQ2(1,"Corrupted PETSc matrix: nz given %d actual nz %d",sendcount,cnt);

    /*--------------------------------------------------------------------
       Gather all column indices to all processors
    */
    for (i=0; i<size; i++) {
      recvcounts[i] = 0;
      for (j=a->rowners[i]; j<a->rowners[i+1]; j++) {
        recvcounts[i] += lens[j];
      }
    }
    displs[0]  = 0;
    for (i=1; i<size; i++) {
      displs[i] = displs[i-1] + recvcounts[i-1];
    }
    ierr = MPI_Allgatherv(jsendbuf,sendcount,MPI_INT,b->j,recvcounts,displs,MPI_INT,A->comm);CHKERRQ(ierr);

    /*--------------------------------------------------------------------
        Assemble the matrix into useable form (note numerical values not yet set)
    */
    /* set the b->ilen (length of each row) values */
    ierr = PetscMemcpy(b->ilen,lens,A->M*sizeof(int));CHKERRQ(ierr);
    /* set the b->i indices */
    b->i[0] = 0;
    for (i=1; i<=A->M; i++) {
      b->i[i] = b->i[i-1] + lens[i-1];
    }
    ierr = PetscFree(lens);CHKERRQ(ierr);
    ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  } else {
    B  = **Bin;
    b = (Mat_SeqAIJ *)B->data;
  }

  /*--------------------------------------------------------------------
       Copy my part of matrix numerical values into the values location 
  */
  sendcount = ad->nz + bd->nz;
  sendbuf   = b->a + b->i[rstarts[rank]];
  a_sendbuf = ad->a;
  b_sendbuf = bd->a;
  b_sendj   = bd->j;
  n         = a->rend - a->rstart;
  cnt       = 0;
  for (i=0; i<n; i++) {

    /* put in lower diagonal portion */
    m = bd->i[i+1] - bd->i[i];
    while (m > 0) {
      /* is it above diagonal (in bd (compressed) numbering) */
      if (garray[*b_sendj] > a->rstart + i) break;
      sendbuf[cnt++] = *b_sendbuf++;
      m--;
      b_sendj++;
    }

    /* put in diagonal portion */
    for (j=ad->i[i]; j<ad->i[i+1]; j++) {
      sendbuf[cnt++] = *a_sendbuf++;
    }

    /* put in upper diagonal portion */
    while (m-- > 0) {
      sendbuf[cnt++] = *b_sendbuf++;
      b_sendj++;
    }
  }
  if (cnt != sendcount) SETERRQ2(1,"Corrupted PETSc matrix: nz given %d actual nz %d",sendcount,cnt);
   
  /* ----------------------------------------------------------------- 
     Gather all numerical values to all processors 
  */
  if (!recvcounts) {
    ierr   = PetscMalloc(2*size*sizeof(int),&recvcounts);CHKERRQ(ierr);
    displs = recvcounts + size;
  }
  for (i=0; i<size; i++) {
    recvcounts[i] = b->i[rstarts[i+1]] - b->i[rstarts[i]];
  }
  displs[0]  = 0;
  for (i=1; i<size; i++) {
    displs[i] = displs[i-1] + recvcounts[i-1];
  }
  recvbuf   = b->a;
  ierr = MPI_Allgatherv(sendbuf,sendcount,MPIU_SCALAR,recvbuf,recvcounts,displs,MPIU_SCALAR,A->comm);CHKERRQ(ierr);
  ierr = PetscFree(recvcounts);CHKERRQ(ierr);
  if (A->symmetric){
    ierr = MatSetOption(B,MAT_SYMMETRIC);CHKERRQ(ierr);
  } else if (A->hermitian) {
    ierr = MatSetOption(B,MAT_HERMITIAN);CHKERRQ(ierr);
  } else if (A->structurally_symmetric) {
    ierr = MatSetOption(B,MAT_STRUCTURALLY_SYMMETRIC);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetSubMatrices_MPIAIJ" 
int MatGetSubMatrices_MPIAIJ(Mat C,int ismax,const IS isrow[],const IS iscol[],MatReuse scall,Mat *submat[])
{ 
  int         nmax,nstages_local,nstages,i,pos,max_no,ierr,nrow,ncol;
  PetscTruth  rowflag,colflag,wantallmatrix = PETSC_FALSE,twantallmatrix;

  PetscFunctionBegin;
  /*
       Check for special case each processor gets entire matrix
  */
  if (ismax == 1 && C->M == C->N) {
    ierr = ISIdentity(*isrow,&rowflag);CHKERRQ(ierr);
    ierr = ISIdentity(*iscol,&colflag);CHKERRQ(ierr);
    ierr = ISGetLocalSize(*isrow,&nrow);CHKERRQ(ierr);
    ierr = ISGetLocalSize(*iscol,&ncol);CHKERRQ(ierr);
    if (rowflag && colflag && nrow == C->M && ncol == C->N) {
      wantallmatrix = PETSC_TRUE;
      ierr = PetscOptionsGetLogical(C->prefix,"-use_fast_submatrix",&wantallmatrix,PETSC_NULL);CHKERRQ(ierr);
    }
  }
  ierr = MPI_Allreduce(&wantallmatrix,&twantallmatrix,1,MPI_INT,MPI_MIN,C->comm);CHKERRQ(ierr);
  if (twantallmatrix) {
    ierr = MatGetSubMatrix_MPIAIJ_All(C,scall,submat);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  /* Allocate memory to hold all the submatrices */
  if (scall != MAT_REUSE_MATRIX) {
    ierr = PetscMalloc((ismax+1)*sizeof(Mat),submat);CHKERRQ(ierr);
  }
  /* Determine the number of stages through which submatrices are done */
  nmax          = 20*1000000 / (C->N * sizeof(int));
  if (!nmax) nmax = 1;
  nstages_local = ismax/nmax + ((ismax % nmax)?1:0);

  /* Make sure every processor loops through the nstages */
  ierr = MPI_Allreduce(&nstages_local,&nstages,1,MPI_INT,MPI_MAX,C->comm);CHKERRQ(ierr);

  for (i=0,pos=0; i<nstages; i++) {
    if (pos+nmax <= ismax) max_no = nmax;
    else if (pos == ismax) max_no = 0;
    else                   max_no = ismax-pos;
    ierr = MatGetSubMatrices_MPIAIJ_Local(C,max_no,isrow+pos,iscol+pos,scall,*submat+pos);CHKERRQ(ierr);
    pos += max_no;
  }
  PetscFunctionReturn(0);
}
/* -------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "MatGetSubMatrices_MPIAIJ_Local" 
int MatGetSubMatrices_MPIAIJ_Local(Mat C,int ismax,const IS isrow[],const IS iscol[],MatReuse scall,Mat *submats)
{ 
  Mat_MPIAIJ  *c = (Mat_MPIAIJ*)C->data;
  Mat         A = c->A;
  Mat_SeqAIJ  *a = (Mat_SeqAIJ*)A->data,*b = (Mat_SeqAIJ*)c->B->data,*mat;
  int         **irow,**icol,*nrow,*ncol,*w1,*w2,*w3,*w4,*rtable,start,end,size;
  int         **sbuf1,**sbuf2,rank,m,i,j,k,l,ct1,ct2,ierr,**rbuf1,row,proc;
  int         nrqs,msz,**ptr,idex,*req_size,*ctr,*pa,*tmp,tcol,nrqr;
  int         **rbuf3,*req_source,**sbuf_aj,**rbuf2,max1,max2,**rmap;
  int         **cmap,**lens,is_no,ncols,*cols,mat_i,*mat_j,tmp2,jmax,*irow_i;
  int         len,ctr_j,*sbuf1_j,*sbuf_aj_i,*rbuf1_i,kmax,*cmap_i,*lens_i;
  int         *rmap_i,tag0,tag1,tag2,tag3;
  MPI_Request *s_waits1,*r_waits1,*s_waits2,*r_waits2,*r_waits3;
  MPI_Request *r_waits4,*s_waits3,*s_waits4;
  MPI_Status  *r_status1,*r_status2,*s_status1,*s_status3,*s_status2;
  MPI_Status  *r_status3,*r_status4,*s_status4;
  MPI_Comm    comm;
  PetscScalar **rbuf4,**sbuf_aa,*vals,*mat_a,*sbuf_aa_i;
  PetscTruth  sorted,eq;
  int         *onodes1,*olengths1;

  PetscFunctionBegin;
  comm   = C->comm;
  tag0   = C->tag;
  size   = c->size;
  rank   = c->rank;
  m      = C->M;
  
  /* Get some new tags to keep the communication clean */
  ierr = PetscObjectGetNewTag((PetscObject)C,&tag1);CHKERRQ(ierr);
  ierr = PetscObjectGetNewTag((PetscObject)C,&tag2);CHKERRQ(ierr);
  ierr = PetscObjectGetNewTag((PetscObject)C,&tag3);CHKERRQ(ierr);

    /* Check if the col indices are sorted */
  for (i=0; i<ismax; i++) {
    ierr = ISSorted(isrow[i],&sorted);CHKERRQ(ierr);
    if (!sorted) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"ISrow is not sorted");
    ierr = ISSorted(iscol[i],&sorted);CHKERRQ(ierr);
    /*    if (!sorted) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"IScol is not sorted"); */
  }

  len    = (2*ismax+1)*(sizeof(int*)+ sizeof(int)) + (m+1)*sizeof(int);
  ierr   = PetscMalloc(len,&irow);CHKERRQ(ierr);
  icol   = irow + ismax;
  nrow   = (int*)(icol + ismax);
  ncol   = nrow + ismax;
  rtable = ncol + ismax;

  for (i=0; i<ismax; i++) { 
    ierr = ISGetIndices(isrow[i],&irow[i]);CHKERRQ(ierr);
    ierr = ISGetIndices(iscol[i],&icol[i]);CHKERRQ(ierr);
    ierr = ISGetLocalSize(isrow[i],&nrow[i]);CHKERRQ(ierr);
    ierr = ISGetLocalSize(iscol[i],&ncol[i]);CHKERRQ(ierr);
  }

  /* Create hash table for the mapping :row -> proc*/
  for (i=0,j=0; i<size; i++) {
    jmax = c->rowners[i+1];
    for (; j<jmax; j++) {
      rtable[j] = i;
    }
  }

  /* evaluate communication - mesg to who, length of mesg, and buffer space
     required. Based on this, buffers are allocated, and data copied into them*/
  ierr   = PetscMalloc(size*4*sizeof(int),&w1);CHKERRQ(ierr); /* mesg size */
  w2     = w1 + size;      /* if w2[i] marked, then a message to proc i*/
  w3     = w2 + size;      /* no of IS that needs to be sent to proc i */
  w4     = w3 + size;      /* temp work space used in determining w1, w2, w3 */
  ierr   = PetscMemzero(w1,size*3*sizeof(int));CHKERRQ(ierr); /* initialize work vector*/
  for (i=0; i<ismax; i++) { 
    ierr   = PetscMemzero(w4,size*sizeof(int));CHKERRQ(ierr); /* initialize work vector*/
    jmax   = nrow[i];
    irow_i = irow[i];
    for (j=0; j<jmax; j++) {
      row  = irow_i[j];
      proc = rtable[row];
      w4[proc]++;
    }
    for (j=0; j<size; j++) { 
      if (w4[j]) { w1[j] += w4[j];  w3[j]++;} 
    }
  }
  
  nrqs     = 0;              /* no of outgoing messages */
  msz      = 0;              /* total mesg length (for all procs) */
  w1[rank] = 0;              /* no mesg sent to self */
  w3[rank] = 0;
  for (i=0; i<size; i++) {
    if (w1[i])  { w2[i] = 1; nrqs++;} /* there exists a message to proc i */
  }
  ierr = PetscMalloc((nrqs+1)*sizeof(int),&pa);CHKERRQ(ierr); /*(proc -array)*/
  for (i=0,j=0; i<size; i++) {
    if (w1[i]) { pa[j] = i; j++; }
  } 

  /* Each message would have a header = 1 + 2*(no of IS) + data */
  for (i=0; i<nrqs; i++) {
    j     = pa[i];
    w1[j] += w2[j] + 2* w3[j];   
    msz   += w1[j];  
  }

  /* Determine the number of messages to expect, their lengths, from from-ids */
  ierr = PetscGatherNumberOfMessages(comm,w2,w1,&nrqr);CHKERRQ(ierr);
  ierr = PetscGatherMessageLengths(comm,nrqs,nrqr,w1,&onodes1,&olengths1);CHKERRQ(ierr);

  /* Now post the Irecvs corresponding to these messages */
  ierr = PetscPostIrecvInt(comm,tag0,nrqr,onodes1,olengths1,&rbuf1,&r_waits1);CHKERRQ(ierr);
  
  ierr = PetscFree(onodes1);CHKERRQ(ierr);
  ierr = PetscFree(olengths1);CHKERRQ(ierr);
  
  /* Allocate Memory for outgoing messages */
  len      = 2*size*sizeof(int*) + 2*msz*sizeof(int) + size*sizeof(int);
  ierr     = PetscMalloc(len,&sbuf1);CHKERRQ(ierr);
  ptr      = sbuf1 + size;   /* Pointers to the data in outgoing buffers */
  ierr     = PetscMemzero(sbuf1,2*size*sizeof(int*));CHKERRQ(ierr);
  /* allocate memory for outgoing data + buf to receive the first reply */
  tmp      = (int*)(ptr + size);
  ctr      = tmp + 2*msz;

  {
    int *iptr = tmp,ict = 0;
    for (i=0; i<nrqs; i++) {
      j         = pa[i];
      iptr     += ict;
      sbuf1[j]  = iptr;
      ict       = w1[j];
    }
  }

  /* Form the outgoing messages */
  /* Initialize the header space */
  for (i=0; i<nrqs; i++) {
    j           = pa[i];
    sbuf1[j][0] = 0;
    ierr        = PetscMemzero(sbuf1[j]+1,2*w3[j]*sizeof(int));CHKERRQ(ierr);
    ptr[j]      = sbuf1[j] + 2*w3[j] + 1;
  }
  
  /* Parse the isrow and copy data into outbuf */
  for (i=0; i<ismax; i++) {
    ierr   = PetscMemzero(ctr,size*sizeof(int));CHKERRQ(ierr);
    irow_i = irow[i];
    jmax   = nrow[i];
    for (j=0; j<jmax; j++) {  /* parse the indices of each IS */
      row  = irow_i[j];
      proc = rtable[row];
      if (proc != rank) { /* copy to the outgoing buf*/
        ctr[proc]++;
        *ptr[proc] = row;
        ptr[proc]++;
      }
    }
    /* Update the headers for the current IS */
    for (j=0; j<size; j++) { /* Can Optimise this loop too */
      if ((ctr_j = ctr[j])) {
        sbuf1_j        = sbuf1[j];
        k              = ++sbuf1_j[0];
        sbuf1_j[2*k]   = ctr_j;
        sbuf1_j[2*k-1] = i;
      }
    }
  }

  /*  Now  post the sends */
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Request),&s_waits1);CHKERRQ(ierr);
  for (i=0; i<nrqs; ++i) {
    j    = pa[i];
    ierr = MPI_Isend(sbuf1[j],w1[j],MPI_INT,j,tag0,comm,s_waits1+i);CHKERRQ(ierr);
  }

  /* Post Receives to capture the buffer size */
  ierr     = PetscMalloc((nrqs+1)*sizeof(MPI_Request),&r_waits2);CHKERRQ(ierr);
  ierr     = PetscMalloc((nrqs+1)*sizeof(int *),&rbuf2);CHKERRQ(ierr);
  rbuf2[0] = tmp + msz;
  for (i=1; i<nrqs; ++i) {
    rbuf2[i] = rbuf2[i-1]+w1[pa[i-1]];
  }
  for (i=0; i<nrqs; ++i) {
    j    = pa[i];
    ierr = MPI_Irecv(rbuf2[i],w1[j],MPI_INT,j,tag1,comm,r_waits2+i);CHKERRQ(ierr);
  }

  /* Send to other procs the buf size they should allocate */
 

  /* Receive messages*/
  ierr        = PetscMalloc((nrqr+1)*sizeof(MPI_Request),&s_waits2);CHKERRQ(ierr);
  ierr        = PetscMalloc((nrqr+1)*sizeof(MPI_Status),&r_status1);CHKERRQ(ierr);
  len         = 2*nrqr*sizeof(int) + (nrqr+1)*sizeof(int*);
  ierr        = PetscMalloc(len,&sbuf2);CHKERRQ(ierr);
  req_size    = (int*)(sbuf2 + nrqr);
  req_source  = req_size + nrqr;
 
  {
    Mat_SeqAIJ *sA = (Mat_SeqAIJ*)c->A->data,*sB = (Mat_SeqAIJ*)c->B->data;
    int        *sAi = sA->i,*sBi = sB->i,id,rstart = c->rstart;
    int        *sbuf2_i;

    for (i=0; i<nrqr; ++i) {
      ierr = MPI_Waitany(nrqr,r_waits1,&idex,r_status1+i);CHKERRQ(ierr);
      req_size[idex] = 0;
      rbuf1_i         = rbuf1[idex];
      start           = 2*rbuf1_i[0] + 1;
      ierr            = MPI_Get_count(r_status1+i,MPI_INT,&end);CHKERRQ(ierr);
      ierr            = PetscMalloc((end+1)*sizeof(int),&sbuf2[idex]);CHKERRQ(ierr);
      sbuf2_i         = sbuf2[idex];
      for (j=start; j<end; j++) {
        id               = rbuf1_i[j] - rstart;
        ncols            = sAi[id+1] - sAi[id] + sBi[id+1] - sBi[id];
        sbuf2_i[j]       = ncols;
        req_size[idex] += ncols;
      }
      req_source[idex] = r_status1[i].MPI_SOURCE;
      /* form the header */
      sbuf2_i[0]   = req_size[idex];
      for (j=1; j<start; j++) { sbuf2_i[j] = rbuf1_i[j]; }
      ierr = MPI_Isend(sbuf2_i,end,MPI_INT,req_source[idex],tag1,comm,s_waits2+i);CHKERRQ(ierr);
    }
  }
  ierr = PetscFree(r_status1);CHKERRQ(ierr);
  ierr = PetscFree(r_waits1);CHKERRQ(ierr);

  /*  recv buffer sizes */
  /* Receive messages*/
  
  ierr = PetscMalloc((nrqs+1)*sizeof(int*),&rbuf3);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqs+1)*sizeof(PetscScalar*),&rbuf4);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Request),&r_waits3);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Request),&r_waits4);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Status),&r_status2);CHKERRQ(ierr);

  for (i=0; i<nrqs; ++i) {
    ierr = MPI_Waitany(nrqs,r_waits2,&idex,r_status2+i);CHKERRQ(ierr);
    ierr = PetscMalloc((rbuf2[idex][0]+1)*sizeof(int),&rbuf3[idex]);CHKERRQ(ierr);
    ierr = PetscMalloc((rbuf2[idex][0]+1)*sizeof(PetscScalar),&rbuf4[idex]);CHKERRQ(ierr);
    ierr = MPI_Irecv(rbuf3[idex],rbuf2[idex][0],MPI_INT,r_status2[i].MPI_SOURCE,tag2,comm,r_waits3+idex);CHKERRQ(ierr);
    ierr = MPI_Irecv(rbuf4[idex],rbuf2[idex][0],MPIU_SCALAR,r_status2[i].MPI_SOURCE,tag3,comm,r_waits4+idex);CHKERRQ(ierr);
  } 
  ierr = PetscFree(r_status2);CHKERRQ(ierr);
  ierr = PetscFree(r_waits2);CHKERRQ(ierr);
  
  /* Wait on sends1 and sends2 */
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Status),&s_status1);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Status),&s_status2);CHKERRQ(ierr);

  ierr = MPI_Waitall(nrqs,s_waits1,s_status1);CHKERRQ(ierr);
  ierr = MPI_Waitall(nrqr,s_waits2,s_status2);CHKERRQ(ierr);
  ierr = PetscFree(s_status1);CHKERRQ(ierr);
  ierr = PetscFree(s_status2);CHKERRQ(ierr);
  ierr = PetscFree(s_waits1);CHKERRQ(ierr);
  ierr = PetscFree(s_waits2);CHKERRQ(ierr);

  /* Now allocate buffers for a->j, and send them off */
  ierr = PetscMalloc((nrqr+1)*sizeof(int*),&sbuf_aj);CHKERRQ(ierr);
  for (i=0,j=0; i<nrqr; i++) j += req_size[i];
  ierr = PetscMalloc((j+1)*sizeof(int),&sbuf_aj[0]);CHKERRQ(ierr);
  for (i=1; i<nrqr; i++)  sbuf_aj[i] = sbuf_aj[i-1] + req_size[i-1];
  
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Request),&s_waits3);CHKERRQ(ierr);
  {
    int nzA,nzB,*a_i = a->i,*b_i = b->i,imark;
    int *cworkA,*cworkB,cstart = c->cstart,rstart = c->rstart,*bmap = c->garray;
    int *a_j = a->j,*b_j = b->j,ctmp;

    for (i=0; i<nrqr; i++) {
      rbuf1_i   = rbuf1[i]; 
      sbuf_aj_i = sbuf_aj[i];
      ct1       = 2*rbuf1_i[0] + 1;
      ct2       = 0;
      for (j=1,max1=rbuf1_i[0]; j<=max1; j++) { 
        kmax = rbuf1[i][2*j];
        for (k=0; k<kmax; k++,ct1++) {
          row    = rbuf1_i[ct1] - rstart;
          nzA    = a_i[row+1] - a_i[row];     nzB = b_i[row+1] - b_i[row];
          ncols  = nzA + nzB;
          cworkA = a_j + a_i[row]; cworkB = b_j + b_i[row];

          /* load the column indices for this row into cols*/
          cols  = sbuf_aj_i + ct2;
          
          for (l=0; l<nzB; l++) {
            if ((ctmp = bmap[cworkB[l]]) < cstart)  cols[l] = ctmp;
            else break;
          }
          imark = l;
          for (l=0; l<nzA; l++)   cols[imark+l] = cstart + cworkA[l];
          for (l=imark; l<nzB; l++) cols[nzA+l] = bmap[cworkB[l]];

          ct2 += ncols;
        }
      }
      ierr = MPI_Isend(sbuf_aj_i,req_size[i],MPI_INT,req_source[i],tag2,comm,s_waits3+i);CHKERRQ(ierr);
    }
  } 
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Status),&r_status3);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Status),&s_status3);CHKERRQ(ierr);

  /* Allocate buffers for a->a, and send them off */
  ierr = PetscMalloc((nrqr+1)*sizeof(PetscScalar*),&sbuf_aa);CHKERRQ(ierr);
  for (i=0,j=0; i<nrqr; i++) j += req_size[i];
  ierr = PetscMalloc((j+1)*sizeof(PetscScalar),&sbuf_aa[0]);CHKERRQ(ierr);
  for (i=1; i<nrqr; i++)  sbuf_aa[i] = sbuf_aa[i-1] + req_size[i-1];
  
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Request),&s_waits4);CHKERRQ(ierr);
  {
    int    nzA,nzB,*a_i = a->i,*b_i = b->i, *cworkB,imark;
    int    cstart = c->cstart,rstart = c->rstart,*bmap = c->garray;
    int    *b_j = b->j;
    PetscScalar *vworkA,*vworkB,*a_a = a->a,*b_a = b->a;
    
    for (i=0; i<nrqr; i++) {
      rbuf1_i   = rbuf1[i];
      sbuf_aa_i = sbuf_aa[i];
      ct1       = 2*rbuf1_i[0]+1;
      ct2       = 0;
      for (j=1,max1=rbuf1_i[0]; j<=max1; j++) {
        kmax = rbuf1_i[2*j];
        for (k=0; k<kmax; k++,ct1++) {
          row    = rbuf1_i[ct1] - rstart;
          nzA    = a_i[row+1] - a_i[row];     nzB = b_i[row+1] - b_i[row];
          ncols  = nzA + nzB;
          cworkB = b_j + b_i[row];
          vworkA = a_a + a_i[row]; 
          vworkB = b_a + b_i[row];

          /* load the column values for this row into vals*/
          vals  = sbuf_aa_i+ct2;
          
          for (l=0; l<nzB; l++) {
            if ((bmap[cworkB[l]]) < cstart)  vals[l] = vworkB[l];
            else break;
          }
          imark = l;
          for (l=0; l<nzA; l++)   vals[imark+l] = vworkA[l];
          for (l=imark; l<nzB; l++) vals[nzA+l] = vworkB[l];
          
          ct2 += ncols;
        }
      }
      ierr = MPI_Isend(sbuf_aa_i,req_size[i],MPIU_SCALAR,req_source[i],tag3,comm,s_waits4+i);CHKERRQ(ierr);
    }
  } 
  ierr = PetscMalloc((nrqs+1)*sizeof(MPI_Status),&r_status4);CHKERRQ(ierr);
  ierr = PetscMalloc((nrqr+1)*sizeof(MPI_Status),&s_status4);CHKERRQ(ierr);
  ierr = PetscFree(rbuf1);CHKERRQ(ierr);

  /* Form the matrix */
  /* create col map */
  {
    int *icol_i;
    
    len     = (1+ismax)*sizeof(int*)+ ismax*C->N*sizeof(int);
    ierr    = PetscMalloc(len,&cmap);CHKERRQ(ierr);
    cmap[0] = (int *)(cmap + ismax);
    ierr    = PetscMemzero(cmap[0],(1+ismax*C->N)*sizeof(int));CHKERRQ(ierr);
    for (i=1; i<ismax; i++) { cmap[i] = cmap[i-1] + C->N; }
    for (i=0; i<ismax; i++) {
      jmax   = ncol[i];
      icol_i = icol[i];
      cmap_i = cmap[i];
      for (j=0; j<jmax; j++) { 
        cmap_i[icol_i[j]] = j+1; 
      }
    }
  }

  /* Create lens which is required for MatCreate... */
  for (i=0,j=0; i<ismax; i++) { j += nrow[i]; }
  len     = (1+ismax)*sizeof(int*)+ j*sizeof(int);
  ierr    = PetscMalloc(len,&lens);CHKERRQ(ierr);
  lens[0] = (int *)(lens + ismax);
  ierr    = PetscMemzero(lens[0],j*sizeof(int));CHKERRQ(ierr);
  for (i=1; i<ismax; i++) { lens[i] = lens[i-1] + nrow[i-1]; }
  
  /* Update lens from local data */
  for (i=0; i<ismax; i++) {
    jmax   = nrow[i];
    cmap_i = cmap[i];
    irow_i = irow[i];
    lens_i = lens[i];
    for (j=0; j<jmax; j++) {
      row  = irow_i[j];
      proc = rtable[row];
      if (proc == rank) {
        ierr = MatGetRow_MPIAIJ(C,row,&ncols,&cols,0);CHKERRQ(ierr);
        for (k=0; k<ncols; k++) {
          if (cmap_i[cols[k]]) { lens_i[j]++;}
        }
        ierr = MatRestoreRow_MPIAIJ(C,row,&ncols,&cols,0);CHKERRQ(ierr);
      }
    }
  }
  
  /* Create row map*/
  len     = (1+ismax)*sizeof(int*)+ ismax*C->M*sizeof(int);
  ierr    = PetscMalloc(len,&rmap);CHKERRQ(ierr);
  rmap[0] = (int *)(rmap + ismax);
  ierr    = PetscMemzero(rmap[0],ismax*C->M*sizeof(int));CHKERRQ(ierr);
  for (i=1; i<ismax; i++) { rmap[i] = rmap[i-1] + C->M;}
  for (i=0; i<ismax; i++) {
    rmap_i = rmap[i];
    irow_i = irow[i];
    jmax   = nrow[i];
    for (j=0; j<jmax; j++) { 
      rmap_i[irow_i[j]] = j; 
    }
  }
 
  /* Update lens from offproc data */
  {
    int *rbuf2_i,*rbuf3_i,*sbuf1_i;

    for (tmp2=0; tmp2<nrqs; tmp2++) {
      ierr = MPI_Waitany(nrqs,r_waits3,&i,r_status3+tmp2);CHKERRQ(ierr);
      idex   = pa[i];
      sbuf1_i = sbuf1[idex];
      jmax    = sbuf1_i[0];
      ct1     = 2*jmax+1; 
      ct2     = 0;               
      rbuf2_i = rbuf2[i];
      rbuf3_i = rbuf3[i];
      for (j=1; j<=jmax; j++) {
        is_no   = sbuf1_i[2*j-1];
        max1    = sbuf1_i[2*j];
        lens_i  = lens[is_no];
        cmap_i  = cmap[is_no];
        rmap_i  = rmap[is_no];
        for (k=0; k<max1; k++,ct1++) {
          row  = rmap_i[sbuf1_i[ct1]]; /* the val in the new matrix to be */
          max2 = rbuf2_i[ct1];
          for (l=0; l<max2; l++,ct2++) {
            if (cmap_i[rbuf3_i[ct2]]) {
              lens_i[row]++;
            }
          }
        }
      }
    }
  }    
  ierr = PetscFree(r_status3);CHKERRQ(ierr);
  ierr = PetscFree(r_waits3);CHKERRQ(ierr);
  ierr = MPI_Waitall(nrqr,s_waits3,s_status3);CHKERRQ(ierr);
  ierr = PetscFree(s_status3);CHKERRQ(ierr);
  ierr = PetscFree(s_waits3);CHKERRQ(ierr);

  /* Create the submatrices */
  if (scall == MAT_REUSE_MATRIX) {
    PetscTruth flag;

    /*
        Assumes new rows are same length as the old rows,hence bug!
    */
    for (i=0; i<ismax; i++) {
      mat = (Mat_SeqAIJ *)(submats[i]->data);
      if ((submats[i]->m != nrow[i]) || (submats[i]->n != ncol[i])) {
        SETERRQ(PETSC_ERR_ARG_SIZ,"Cannot reuse matrix. wrong size");
      }
      ierr = PetscMemcmp(mat->ilen,lens[i],submats[i]->m*sizeof(int),&flag);CHKERRQ(ierr);
      if (flag == PETSC_FALSE) {
        SETERRQ(PETSC_ERR_ARG_SIZ,"Cannot reuse matrix. wrong no of nonzeros");
      }
      /* Initial matrix as if empty */
      ierr = PetscMemzero(mat->ilen,submats[i]->m*sizeof(int));CHKERRQ(ierr);
      submats[i]->factor = C->factor;
    }
  } else {
    for (i=0; i<ismax; i++) {
      ierr = MatCreate(PETSC_COMM_SELF,nrow[i],ncol[i],PETSC_DETERMINE,PETSC_DETERMINE,submats+i);CHKERRQ(ierr);
      ierr = MatSetType(submats[i],A->type_name);CHKERRQ(ierr);
      ierr = MatSeqAIJSetPreallocation(submats[i],0,lens[i]);CHKERRQ(ierr);
    }
  }

  /* Assemble the matrices */
  /* First assemble the local rows */
  {
    int    ilen_row,*imat_ilen,*imat_j,*imat_i,old_row;
    PetscScalar *imat_a;
  
    for (i=0; i<ismax; i++) {
      mat       = (Mat_SeqAIJ*)submats[i]->data;
      imat_ilen = mat->ilen;
      imat_j    = mat->j;
      imat_i    = mat->i;
      imat_a    = mat->a;
      cmap_i    = cmap[i];
      rmap_i    = rmap[i];
      irow_i    = irow[i];
      jmax      = nrow[i];
      for (j=0; j<jmax; j++) {
        row      = irow_i[j];
        proc     = rtable[row];
        if (proc == rank) {
          old_row  = row;
          row      = rmap_i[row];
          ilen_row = imat_ilen[row];
          ierr     = MatGetRow_MPIAIJ(C,old_row,&ncols,&cols,&vals);CHKERRQ(ierr);
          mat_i    = imat_i[row] ;
          mat_a    = imat_a + mat_i;
          mat_j    = imat_j + mat_i;
          for (k=0; k<ncols; k++) {
            if ((tcol = cmap_i[cols[k]])) { 
              *mat_j++ = tcol - 1;
              *mat_a++ = vals[k];
              ilen_row++;
            }
          }
          ierr = MatRestoreRow_MPIAIJ(C,old_row,&ncols,&cols,&vals);CHKERRQ(ierr);
          imat_ilen[row] = ilen_row; 
        }
      }
    }
  }

  /*   Now assemble the off proc rows*/
  {
    int    *sbuf1_i,*rbuf2_i,*rbuf3_i,*imat_ilen,ilen;
    int    *imat_j,*imat_i;
    PetscScalar *imat_a,*rbuf4_i;

    for (tmp2=0; tmp2<nrqs; tmp2++) {
      ierr = MPI_Waitany(nrqs,r_waits4,&i,r_status4+tmp2);CHKERRQ(ierr);
      idex   = pa[i];
      sbuf1_i = sbuf1[idex];
      jmax    = sbuf1_i[0];           
      ct1     = 2*jmax + 1; 
      ct2     = 0;    
      rbuf2_i = rbuf2[i];
      rbuf3_i = rbuf3[i];
      rbuf4_i = rbuf4[i];
      for (j=1; j<=jmax; j++) {
        is_no     = sbuf1_i[2*j-1];
        rmap_i    = rmap[is_no];
        cmap_i    = cmap[is_no];
        mat       = (Mat_SeqAIJ*)submats[is_no]->data;
        imat_ilen = mat->ilen;
        imat_j    = mat->j;
        imat_i    = mat->i;
        imat_a    = mat->a;
        max1      = sbuf1_i[2*j];
        for (k=0; k<max1; k++,ct1++) {
          row   = sbuf1_i[ct1];
          row   = rmap_i[row]; 
          ilen  = imat_ilen[row];
          mat_i = imat_i[row] ;
          mat_a = imat_a + mat_i;
          mat_j = imat_j + mat_i;
          max2 = rbuf2_i[ct1];
          for (l=0; l<max2; l++,ct2++) {
            if ((tcol = cmap_i[rbuf3_i[ct2]])) {
              *mat_j++ = tcol - 1;
              *mat_a++ = rbuf4_i[ct2];
              ilen++;
            }
          }
          imat_ilen[row] = ilen;
        }
      }
    }
  }    
  ierr = PetscFree(r_status4);CHKERRQ(ierr);
  ierr = PetscFree(r_waits4);CHKERRQ(ierr);
  ierr = MPI_Waitall(nrqr,s_waits4,s_status4);CHKERRQ(ierr);
  ierr = PetscFree(s_waits4);CHKERRQ(ierr);
  ierr = PetscFree(s_status4);CHKERRQ(ierr);

  /* Restore the indices */
  for (i=0; i<ismax; i++) {
    ierr = ISRestoreIndices(isrow[i],irow+i);CHKERRQ(ierr);
    ierr = ISRestoreIndices(iscol[i],icol+i);CHKERRQ(ierr);
  }

  /* Destroy allocated memory */
  ierr = PetscFree(irow);CHKERRQ(ierr);
  ierr = PetscFree(w1);CHKERRQ(ierr);
  ierr = PetscFree(pa);CHKERRQ(ierr);

  ierr = PetscFree(sbuf1);CHKERRQ(ierr);
  ierr = PetscFree(rbuf2);CHKERRQ(ierr);
  for (i=0; i<nrqr; ++i) {
    ierr = PetscFree(sbuf2[i]);CHKERRQ(ierr);
  }
  for (i=0; i<nrqs; ++i) {
    ierr = PetscFree(rbuf3[i]);CHKERRQ(ierr);
    ierr = PetscFree(rbuf4[i]);CHKERRQ(ierr);
  }

  ierr = PetscFree(sbuf2);CHKERRQ(ierr);
  ierr = PetscFree(rbuf3);CHKERRQ(ierr);
  ierr = PetscFree(rbuf4);CHKERRQ(ierr);
  ierr = PetscFree(sbuf_aj[0]);CHKERRQ(ierr);
  ierr = PetscFree(sbuf_aj);CHKERRQ(ierr);
  ierr = PetscFree(sbuf_aa[0]);CHKERRQ(ierr);
  ierr = PetscFree(sbuf_aa);CHKERRQ(ierr);
  
  ierr = PetscFree(cmap);CHKERRQ(ierr);
  ierr = PetscFree(rmap);CHKERRQ(ierr);
  ierr = PetscFree(lens);CHKERRQ(ierr);

  for (i=0; i<ismax; i++) {
    ierr = MatAssemblyBegin(submats[i],MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(submats[i],MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    if (A->symmetric || A->structurally_symmetric || A->hermitian) {
      ierr = ISEqual(isrow[i],iscol[i],&eq);CHKERRQ(ierr);
      if (eq) {
	if (A->symmetric){
	  ierr = MatSetOption(submats[i],MAT_SYMMETRIC);CHKERRQ(ierr);
	} else if (A->hermitian) {
	  ierr = MatSetOption(submats[i],MAT_HERMITIAN);CHKERRQ(ierr);
	} else if (A->structurally_symmetric) {
	  ierr = MatSetOption(submats[i],MAT_STRUCTURALLY_SYMMETRIC);CHKERRQ(ierr);
	}
      }
    }
  }
  PetscFunctionReturn(0);
}

