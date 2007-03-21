static char help[] = "Creates and outputs a structured mesh.\n\n";

#include "petscmesh.h"
#include <CartesianSieve.hh>

using ALE::Obj;

typedef struct {
  int       debug;      // The debugging level
  PetscInt  dim;        // The topological mesh dimension
  PetscInt *numCells;   // The number of cells in each dimension
  PetscInt *partitions; // The number of divisions in each dimension
} Options;

#undef __FUNCT__
#define __FUNCT__ "ProcessOptions"
PetscErrorCode ProcessOptions(MPI_Comm comm, Options *options)
{
  PetscInt       n;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  options->debug = 0;
  options->dim   = 2;

  ierr = PetscOptionsBegin(comm, "", "Options for mesh loading", "DMMG");CHKERRQ(ierr);
    ierr = PetscOptionsInt("-debug", "The debugging level", "ex11.cxx", options->debug, &options->debug, PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscOptionsInt("-dim", "The topological mesh dimension", "ex11.cxx", options->dim, &options->dim, PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscMalloc(options->dim * sizeof(PetscInt), &options->numCells);CHKERRQ(ierr);
    for(int d = 0; d < options->dim; ++d) options->numCells[d] = 1;
    n = options->dim;
    ierr = PetscOptionsIntArray("-num_cells", "The number of cells in each dimension", "ex11.cxx", options->numCells, &n, PETSC_NULL);
    ierr = PetscMalloc(options->dim * sizeof(PetscInt), &options->partitions);CHKERRQ(ierr);
    for(int d = 0; d < options->dim; ++d) options->partitions[d] = 1;
    n = options->dim;
    ierr = PetscOptionsIntArray("-partitions", "The number of divisions in each dimension", "ex11.cxx", options->partitions, &n, PETSC_NULL);
  ierr = PetscOptionsEnd();
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "CreateMesh"
PetscErrorCode CreateMesh(MPI_Comm comm, Options *options, Mesh *mesh)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ALE::LogStage stage = ALE::LogStageRegister("MeshCreation");
  ALE::LogStagePush(stage);
  ierr = PetscPrintf(comm, "Creating mesh\n");CHKERRQ(ierr);
  ierr = MeshCreate(comm, mesh);CHKERRQ(ierr);
  Obj<ALE::CartesianTopology<int> > t = ALE::CartesianMeshBuilder::createCartesianMesh(comm, options->dim, options->numCells, options->partitions, options->debug);
  t->view("");
  //ierr = MeshSetMesh(*mesh, m);CHKERRQ(ierr);
  ALE::LogStagePop(stage);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char *argv[])
{
  MPI_Comm       comm;
  Options        options;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscInitialize(&argc, &argv, (char *) 0, help);CHKERRQ(ierr);
  comm = PETSC_COMM_WORLD;
  try {
    Mesh mesh;

    ierr = ProcessOptions(comm, &options);CHKERRQ(ierr);
    ierr = CreateMesh(comm, &options, &mesh);CHKERRQ(ierr);
    ierr = MeshDestroy(mesh);CHKERRQ(ierr);
  } catch (ALE::Exception e) {
    std::cout << e << std::endl;
  }
  ierr = PetscFinalize();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
