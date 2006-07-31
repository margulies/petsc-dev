#include "src/dm/mesh/meshpylith.h"   /*I      "petscmesh.h"   I*/

namespace ALE {
  namespace PyLith {
    using ALE::Mesh;
    //
    // Builder methods
    //
    inline void Builder::ignoreComments(char *buf, PetscInt bufSize, FILE *f) {
      while((fgets(buf, bufSize, f) != NULL) && ((buf[0] == '#') || (buf[0] == '\0'))) {}
    };
    void Builder::readConnectivity(MPI_Comm comm, const std::string& filename, int& corners, const bool useZeroBase, int& numElements, int *vertices[], int *materials[]) {
      PetscViewer    viewer;
      FILE          *f;
      PetscInt       maxCells = 1024, cellCount = 0;
      PetscInt      *verts;
      PetscInt      *mats;
      char           buf[2048];
      PetscInt       c;
      PetscInt       commRank;
      PetscErrorCode ierr;

      ierr = MPI_Comm_rank(comm, &commRank);
      if (commRank != 0) return;
      ierr = PetscViewerCreate(PETSC_COMM_SELF, &viewer);
      ierr = PetscViewerSetType(viewer, PETSC_VIEWER_ASCII);
      ierr = PetscViewerFileSetMode(viewer, FILE_MODE_READ);
      ierr = PetscViewerFileSetName(viewer, filename.c_str());
      ierr = PetscViewerASCIIGetPointer(viewer, &f);
      /* Ignore comments */
      ignoreComments(buf, 2048, f);
      do {
        const char *v = strtok(buf, " ");
        int         elementType;

        if (cellCount == maxCells) {
          PetscInt *vtmp, *mtmp;

          vtmp = verts;
          mtmp = mats;
          ierr = PetscMalloc2(maxCells*2*corners,PetscInt,&verts,maxCells*2,PetscInt,&mats);
          ierr = PetscMemcpy(verts, vtmp, maxCells*corners * sizeof(PetscInt));
          ierr = PetscMemcpy(mats,  mtmp, maxCells         * sizeof(PetscInt));
          ierr = PetscFree2(vtmp,mtmp);
          maxCells *= 2;
        }
        /* Ignore cell number */
        v = strtok(NULL, " ");
        /* Get element type */
        elementType = atoi(v);
        if (elementType == 1) {
          corners = 8;
        } else if (elementType == 5) {
          corners = 4;
        } else {
          ostringstream msg;

          msg << "We do not accept element type " << elementType << " right now";
          throw ALE::Exception(msg.str().c_str());
        }
        if (cellCount == 0) {
          ierr = PetscMalloc2(maxCells*corners,PetscInt,&verts,maxCells,PetscInt,&mats);
        }
        v = strtok(NULL, " ");
        /* Store material type */
        mats[cellCount] = atoi(v);
        v = strtok(NULL, " ");
        /* Ignore infinite domain element code */
        v = strtok(NULL, " ");
        for(c = 0; c < corners; c++) {
          int vertex = atoi(v);
        
          if (!useZeroBase) vertex -= 1;
          verts[cellCount*corners+c] = vertex;
          v = strtok(NULL, " ");
        }
        cellCount++;
      } while(fgets(buf, 2048, f) != NULL);
      ierr = PetscViewerDestroy(viewer);
      numElements = cellCount;
      *vertices   = verts;
      *materials  = mats;
    };
    void Builder::readCoordinates(MPI_Comm comm, const std::string& filename, const int dim, int& numVertices, double *coordinates[]) {
      PetscViewer    viewer;
      FILE          *f;
      PetscInt       maxVerts = 1024, vertexCount = 0;
      PetscScalar   *coords;
      double         scaleFactor = 1.0;
      char           buf[2048];
      PetscInt       c;
      PetscInt       commRank;
      PetscErrorCode ierr;

      ierr = MPI_Comm_rank(comm, &commRank);
      if (commRank == 0) {
        ierr = PetscViewerCreate(PETSC_COMM_SELF, &viewer);
        ierr = PetscViewerSetType(viewer, PETSC_VIEWER_ASCII);
        ierr = PetscViewerFileSetMode(viewer, FILE_MODE_READ);
        ierr = PetscViewerFileSetName(viewer, filename.c_str());
        ierr = PetscViewerASCIIGetPointer(viewer, &f);
        /* Ignore comments */
        ignoreComments(buf, 2048, f);
        ierr = PetscMalloc(maxVerts*dim * sizeof(PetscScalar), &coords);
        /* Read units */
        const char *units = strtok(buf, " ");
        if (strcmp(units, "coord_units")) {
          throw ALE::Exception("Invalid coordinate units line");
        }
        units = strtok(NULL, " ");
        if (strcmp(units, "=")) {
          throw ALE::Exception("Invalid coordinate units line");
        }
        units = strtok(NULL, " ");
        if (!strcmp(units, "km")) {
          /* Should use Pythia to do units conversion */
          scaleFactor = 1000.0;
        }
        /* Ignore comments */
        ignoreComments(buf, 2048, f);
        do {
          const char *x = strtok(buf, " ");

          if (vertexCount == maxVerts) {
            PetscScalar *ctmp;

            ctmp = coords;
            ierr = PetscMalloc(maxVerts*2*dim * sizeof(PetscScalar), &coords);
            ierr = PetscMemcpy(coords, ctmp, maxVerts*dim * sizeof(PetscScalar));
            ierr = PetscFree(ctmp);
            maxVerts *= 2;
          }
          /* Ignore vertex number */
          x = strtok(NULL, " ");
          for(c = 0; c < dim; c++) {
            coords[vertexCount*dim+c] = atof(x)*scaleFactor;
            x = strtok(NULL, " ");
          }
          vertexCount++;
        } while(fgets(buf, 2048, f) != NULL);
        ierr = PetscViewerDestroy(viewer);
        numVertices = vertexCount;
        *coordinates = coords;
      }
    };
    void Builder::readSplit(MPI_Comm comm, const std::string& filename, const int dim, const bool useZeroBase, int& numSplit, int *splitInd[], double *splitValues[]) {
      PetscViewer    viewer;
      FILE          *f;
      PetscInt       maxSplit = 1024, splitCount = 0;
      PetscInt      *splitId;
      PetscScalar   *splitVal;
      char           buf[2048];
      PetscInt       c;
      PetscInt       commRank;
      PetscErrorCode ierr;

      ierr = MPI_Comm_rank(comm, &commRank);
      if (dim != 3) {
        throw ALE::Exception("PyLith only works in 3D");
      }
      if (commRank != 0) return;
      ierr = PetscViewerCreate(PETSC_COMM_SELF, &viewer);
      ierr = PetscViewerSetType(viewer, PETSC_VIEWER_ASCII);
      ierr = PetscViewerFileSetMode(viewer, FILE_MODE_READ);
      ierr = PetscExceptionTry1(PetscViewerFileSetName(viewer, filename.c_str()), PETSC_ERR_FILE_OPEN);
      if (PetscExceptionValue(ierr)) {
        // this means that a caller above me has also tryed this exception so I don't handle it here, pass it up
      } else if (PetscExceptionCaught(ierr,PETSC_ERR_FILE_OPEN)) {
        // File does not exist
        return;
      } 
      ierr = PetscViewerASCIIGetPointer(viewer, &f);
      /* Ignore comments */
      ignoreComments(buf, 2048, f);
      ierr = PetscMalloc2(maxSplit*2,PetscInt,&splitId,maxSplit*dim,PetscScalar,&splitVal);
      do {
        const char *s = strtok(buf, " ");

        if (splitCount == maxSplit) {
          PetscInt    *sitmp;
          PetscScalar *svtmp;

          sitmp = splitId;
          svtmp = splitVal;
          ierr = PetscMalloc2(maxSplit*2*2,PetscInt,&splitId,maxSplit*dim*2,PetscScalar,&splitVal);
          ierr = PetscMemcpy(splitId,  sitmp, maxSplit*2   * sizeof(PetscInt));
          ierr = PetscMemcpy(splitVal, svtmp, maxSplit*dim * sizeof(PetscScalar));
          ierr = PetscFree2(sitmp,svtmp);
          maxSplit *= 2;
        }
        /* Get element number */
        int elem = atoi(s);
        if (!useZeroBase) elem -= 1;
        splitId[splitCount*2+0] = elem;
        s = strtok(NULL, " ");
        /* Get node number */
        int node = atoi(s);
        if (!useZeroBase) node -= 1;
        splitId[splitCount*2+1] = node;
        s = strtok(NULL, " ");
        /* Ignore load history number */
        s = strtok(NULL, " ");
        /* Get split values */
        for(c = 0; c < dim; c++) {
          splitVal[splitCount*dim+c] = atof(s);
          s = strtok(NULL, " ");
        }
        splitCount++;
      } while(fgets(buf, 2048, f) != NULL);
      ierr = PetscViewerDestroy(viewer);
      numSplit     = splitCount;
      *splitInd    = splitId;
      *splitValues = splitVal;
    };
    void Builder::createSplitField(int numSplit, int splitInd[], double splitVals[], Obj<Mesh> mesh, Obj<Mesh::field_type> splitField) {
      Obj<Mesh::sieve_type::traits::heightSequence> elements = mesh->getTopology()->heightStratum(0);
      std::map<Mesh::point_type, std::set<int> > elem2vertIndex;
      Obj<std::list<Mesh::point_type> > vertices = std::list<Mesh::point_type>();
      Mesh::field_type::patch_type patch;
      int numElements = elements->size();
      int dim = 3;

      for(int e = 0; e < numSplit; e++) {
        elem2vertIndex[Mesh::point_type(0, splitInd[e*2+0])].insert(e);
      }
      for(std::map<Mesh::point_type, std::set<int> >::iterator e_iter = elem2vertIndex.begin(); e_iter != elem2vertIndex.end(); ++e_iter) {
        vertices->clear();
        for(std::set<int>::iterator v_iter = e_iter->second.begin(); v_iter != e_iter->second.end(); ++v_iter) {
          vertices->push_back(Mesh::point_type(0, numElements+splitInd[*v_iter*2+1]));
        }
        splitField->setPatch(vertices, e_iter->first);

        for(std::list<Mesh::point_type>::iterator v_iter = vertices->begin(); v_iter != vertices->end(); ++v_iter) {
          splitField->setFiberDimension(e_iter->first, *v_iter, dim);
        }
      }
      splitField->orderPatches();
      for(std::map<Mesh::point_type, std::set<int> >::iterator e_iter = elem2vertIndex.begin(); e_iter != elem2vertIndex.end(); ++e_iter) {
        for(std::set<int>::iterator v_iter = e_iter->second.begin(); v_iter != e_iter->second.end(); ++v_iter) {
          splitField->update(e_iter->first, Mesh::point_type(0, numElements+splitInd[*v_iter*2+1]), &splitVals[*v_iter*dim]);
        }
      }
    };
    void Builder::buildCoordinates(const Obj<section_type>& coords, const int embedDim, const double coordinates[]) {
      const section_type::patch_type patch = 0;
      const Obj<topology_type::label_sequence>& vertices = coords->getAtlas()->getTopology()->depthStratum(patch, 0);
      const int numCells = coords->getAtlas()->getTopology()->heightStratum(patch, 0)->size();

      coords->getAtlas()->setFiberDimensionByDepth(patch, 0, embedDim);
      coords->getAtlas()->orderPatches();
      coords->allocate();
      for(topology_type::label_sequence::iterator v_iter = vertices->begin(); v_iter != vertices->end(); ++v_iter) {
        coords->update(patch, *v_iter, &(coordinates[((*v_iter).index - numCells)*embedDim]));
      }
    };
    void Builder::buildMaterials(const Obj<Mesh::section_type>& matField, const int materials[]) {
      const section_type::patch_type patch = 0;
      const Obj<topology_type::label_sequence>& elements = matField->getAtlas()->getTopology()->heightStratum(patch, 0);

      matField->getAtlas()->setFiberDimensionByHeight(patch, 0, 1);
      matField->getAtlas()->orderPatches();
      matField->allocate();
      for(topology_type::label_sequence::iterator e_iter = elements->begin(); e_iter != elements->end(); ++e_iter) {
        double mat = (double) materials[(*e_iter).index];
        matField->update(patch, *e_iter, &mat);
      }
    };
    Obj<Mesh> Builder::readMesh(MPI_Comm comm, const int dim, const std::string& basename, const bool useZeroBase = false, const bool interpolate = false, const int debug = 0) {
      Obj<Mesh>          mesh     = Mesh(comm, dim, debug);
      Obj<sieve_type>    sieve    = new sieve_type(comm, debug);
      Obj<topology_type> topology = new topology_type(comm, debug);
      int    *cells, *materials;
      double *coordinates;
      int    *splitInd;
      double *splitValues;
      int     numCells = 0, numVertices = 0, numCorners = dim+1, numSplit = 0, hasSplit;

      ALE::PyLith::Builder::readConnectivity(comm, basename+".connect", numCorners, useZeroBase, numCells, &cells, &materials);
      ALE::PyLith::Builder::readCoordinates(comm, basename+".coord", dim, numVertices, &coordinates);
      ALE::PyLith::Builder::readSplit(comm, basename+".split", dim, useZeroBase, numSplit, &splitInd, &splitValues);
      ALE::New::SieveBuilder<sieve_type>::buildTopology(sieve, dim, numCells, cells, numVertices, interpolate, numCorners);
      sieve->stratify();
      topology->setPatch(0, sieve);
      topology->stratify();
      mesh->setTopologyNew(topology);
      buildCoordinates(mesh->getSection("coordinates"), dim, coordinates);
      buildMaterials(mesh->getSection("material"), materials);
      MPI_Allreduce(&numSplit, &hasSplit, 1, MPI_INT, MPI_MAX, comm);
      if (hasSplit) {
        createSplitField(numSplit, splitInd, splitValues, mesh, mesh->getField("split"));
      }
      return mesh;
    };
    //
    // Viewer methods
    //
    #undef __FUNCT__  
    #define __FUNCT__ "PyLithWriteVertices"
    PetscErrorCode Viewer::writeVertices(Obj<ALE::Mesh> mesh, PetscViewer viewer) {
      Obj<ALE::Mesh::field_type>   coordinates  = mesh->getCoordinates();
      Obj<ALE::Mesh::bundle_type>  vertexBundle = mesh->getBundle(0);
      ALE::Mesh::field_type::patch_type patch;
      const double  *array = coordinates->restrict(ALE::Mesh::field_type::patch_type());
      int            dim = mesh->getDimension();
      int            numVertices;
      PetscErrorCode ierr;

      PetscFunctionBegin;
      //FIX:
      if (vertexBundle->getGlobalOffsets()) {
        numVertices = vertexBundle->getGlobalOffsets()[mesh->commSize()];
      } else {
        numVertices = mesh->getTopology()->depthStratum(0)->size();
      }
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"coord_units = m\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#  Node      X-coord           Y-coord           Z-coord\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      if (mesh->commRank() == 0) {
        int numLocalVertices = mesh->getTopology()->depthStratum(0)->size();
        int vertexCount = 1;

        for(int v = 0; v < numLocalVertices; v++) {
          ierr = PetscViewerASCIIPrintf(viewer,"%7D ", vertexCount++);CHKERRQ(ierr);
          for(int d = 0; d < dim; d++) {
            if (d > 0) {
              ierr = PetscViewerASCIIPrintf(viewer," ");CHKERRQ(ierr);
            }
            ierr = PetscViewerASCIIPrintf(viewer,"% 16.8E", array[v*dim+d]);CHKERRQ(ierr);
          }
          ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
        }
        for(int p = 1; p < mesh->commSize(); p++) {
          double    *remoteCoords;
          MPI_Status status;

          ierr = MPI_Recv(&numLocalVertices, 1, MPI_INT, p, 1, mesh->comm(), &status);CHKERRQ(ierr);
          ierr = PetscMalloc(numLocalVertices*dim * sizeof(double), &remoteCoords);CHKERRQ(ierr);
          ierr = MPI_Recv(remoteCoords, numLocalVertices*dim, MPI_DOUBLE, p, 1, mesh->comm(), &status);CHKERRQ(ierr);
          for(int v = 0; v < numLocalVertices; v++) {
            ierr = PetscViewerASCIIPrintf(viewer,"%7D   ", vertexCount++);CHKERRQ(ierr);
            for(int d = 0; d < dim; d++) {
              if (d > 0) {
                ierr = PetscViewerASCIIPrintf(viewer, " ");CHKERRQ(ierr);
              }
              ierr = PetscViewerASCIIPrintf(viewer, "% 16.8E", remoteCoords[v*dim+d]);CHKERRQ(ierr);
            }
            ierr = PetscViewerASCIIPrintf(viewer, "\n");CHKERRQ(ierr);
          }
        }
      } else {
        Obj<ALE::Mesh::bundle_type> globalOrder = coordinates->getGlobalOrder();
        Obj<ALE::Mesh::field_type::order_type::coneSequence> cone = globalOrder->getPatch(patch);
        const int *offsets = coordinates->getGlobalOffsets();
        int        numLocalVertices = (offsets[mesh->commRank()+1] - offsets[mesh->commRank()])/dim;
        double    *localCoords;
        int        k = 0;

        ierr = PetscMalloc(numLocalVertices*dim * sizeof(double), &localCoords);CHKERRQ(ierr);
        for(ALE::Mesh::field_type::order_type::coneSequence::iterator p_iter = cone->begin(); p_iter != cone->end(); ++p_iter) {
          int dim = globalOrder->getFiberDimension(patch, *p_iter);

          if (dim > 0) {
            int offset = coordinates->getFiberOffset(patch, *p_iter);

            for(int i = offset; i < offset+dim; ++i) {
              localCoords[k++] = array[i];
            }
          }
        }
        if (k != numLocalVertices*dim) {
          SETERRQ2(PETSC_ERR_PLIB, "Invalid number of coordinates to send %d should be %d", k, numLocalVertices*dim);
        }
        ierr = MPI_Send(&numLocalVertices, 1, MPI_INT, 0, 1, mesh->comm());CHKERRQ(ierr);
        ierr = MPI_Send(localCoords, numLocalVertices*dim, MPI_DOUBLE, 0, 1, mesh->comm());CHKERRQ(ierr);
        ierr = PetscFree(localCoords);CHKERRQ(ierr);
      }
      PetscFunctionReturn(0);
    };
    #undef __FUNCT__  
    #define __FUNCT__ "PyLithWriteElements"
    PetscErrorCode Viewer::writeElements(Obj<ALE::Mesh> mesh, PetscViewer viewer) {
      Obj<ALE::Mesh::sieve_type> topology = mesh->getTopology();
      Obj<ALE::Mesh::sieve_type::traits::heightSequence> elements = topology->heightStratum(0);
      Obj<ALE::Mesh::bundle_type> elementBundle = mesh->getBundle(topology->depth());
      Obj<ALE::Mesh::bundle_type> vertexBundle = mesh->getBundle(0);
      Obj<ALE::Mesh::bundle_type> globalVertex = vertexBundle->getGlobalOrder();
      Obj<ALE::Mesh::bundle_type> globalElement = elementBundle->getGlobalOrder();
      Obj<ALE::Mesh::field_type> material;
      ALE::Mesh::bundle_type::patch_type patch;
      std::string    orderName("element");
      bool           hasMaterial = mesh->hasField("material");
      int            dim  = mesh->getDimension();
      int            corners = topology->nCone(*elements->begin(), topology->depth())->size();
      int            elementType = -1;
      PetscErrorCode ierr;

      PetscFunctionBegin;
      if (dim != 3) {
        SETERRQ(PETSC_ERR_SUP, "PyLith only supports 3D meshes.");
      }
      if (corners == 4) {
        // Linear tetrahedron
        elementType = 5;
      } else if (corners == 8) {
        // Linear hexahedron
        elementType = 1;
      } else {
        SETERRQ1(PETSC_ERR_SUP, "PyLith Error: Unsupported number of elements vertices: %d", corners);
      }
      if (hasMaterial) {
        material = mesh->getField("material");
      }
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#     N ETP MAT INF     N1     N2     N3     N4     N5     N6     N7     N8\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      if (mesh->commRank() == 0) {
        int elementCount = 1;

        for(ALE::Mesh::sieve_type::traits::heightSequence::iterator e_itor = elements->begin(); e_itor != elements->end(); ++e_itor) {
          Obj<ALE::Mesh::bundle_type::order_type::coneSequence> cone = vertexBundle->getPatch(orderName, *e_itor);

          ierr = PetscViewerASCIIPrintf(viewer, "%7d %3d", elementCount++, elementType);CHKERRQ(ierr);
          if (hasMaterial) {
            // No infinite elements
            ierr = PetscViewerASCIIPrintf(viewer, " %3d %3d", (int) material->restrict(patch, *e_itor)[0], 0);CHKERRQ(ierr);
          } else {
            // No infinite elements
            ierr = PetscViewerASCIIPrintf(viewer, " %3d %3d", 1, 0);CHKERRQ(ierr);
          }
          for(ALE::Mesh::bundle_type::order_type::coneSequence::iterator c_itor = cone->begin(); c_itor != cone->end(); ++c_itor) {
            ierr = PetscViewerASCIIPrintf(viewer, " %6d", globalVertex->getIndex(patch, *c_itor).prefix+1);CHKERRQ(ierr);
          }
          ierr = PetscViewerASCIIPrintf(viewer, "\n");CHKERRQ(ierr);
        }
        for(int p = 1; p < mesh->commSize(); p++) {
          int         numLocalElements;
          int        *remoteVertices;
          MPI_Status  status;

          ierr = MPI_Recv(&numLocalElements, 1, MPI_INT, p, 1, mesh->comm(), &status);CHKERRQ(ierr);
          ierr = PetscMalloc(numLocalElements*(corners+1) * sizeof(int), &remoteVertices);CHKERRQ(ierr);
          ierr = MPI_Recv(remoteVertices, numLocalElements*(corners+1), MPI_INT, p, 1, mesh->comm(), &status);CHKERRQ(ierr);
          for(int e = 0; e < numLocalElements; e++) {
            // Only linear tetrahedra, material, no infinite elements
            int mat = remoteVertices[e*(corners+1)+corners];

            ierr = PetscViewerASCIIPrintf(viewer, "%7d %3d %3d %3d", elementCount++, elementType, mat, 0);CHKERRQ(ierr);
            for(int c = 0; c < corners; c++) {
              ierr = PetscViewerASCIIPrintf(viewer, " %6d", remoteVertices[e*(corners+1)+c]);CHKERRQ(ierr);
            }
            ierr = PetscViewerASCIIPrintf(viewer, "\n");CHKERRQ(ierr);
          }
          ierr = PetscFree(remoteVertices);CHKERRQ(ierr);
        }
      } else {
        const int *offsets = elementBundle->getGlobalOffsets();
        int        numLocalElements = offsets[mesh->commRank()+1] - offsets[mesh->commRank()];
        int       *localVertices;
        int        k = 0;

        ierr = PetscMalloc(numLocalElements*(corners+1) * sizeof(int), &localVertices);CHKERRQ(ierr);
        for(ALE::Mesh::sieve_type::traits::heightSequence::iterator e_itor = elements->begin(); e_itor != elements->end(); ++e_itor) {
          Obj<ALE::Mesh::bundle_type::order_type::coneSequence> cone = vertexBundle->getPatch(orderName, *e_itor);

          if (globalElement->getFiberDimension(patch, *e_itor) > 0) {
            for(ALE::Mesh::bundle_type::order_type::coneSequence::iterator c_itor = cone->begin(); c_itor != cone->end(); ++c_itor) {
              localVertices[k++] = globalVertex->getIndex(patch, *c_itor).prefix;
            }
            if (hasMaterial) {
              localVertices[k++] = (int) material->restrict(patch, *e_itor)[0];
            } else {
              localVertices[k++] = 1;
            }
          }
        }
        if (k != numLocalElements*corners) {
          SETERRQ2(PETSC_ERR_PLIB, "Invalid number of vertices to send %d should be %d", k, numLocalElements*corners);
        }
        ierr = MPI_Send(&numLocalElements, 1, MPI_INT, 0, 1, mesh->comm());CHKERRQ(ierr);
        ierr = MPI_Send(localVertices, numLocalElements*(corners+1), MPI_INT, 0, 1, mesh->comm());CHKERRQ(ierr);
        ierr = PetscFree(localVertices);CHKERRQ(ierr);
      }
      PetscFunctionReturn(0);
    };
    #undef __FUNCT__  
    #define __FUNCT__ "PyLithWriteVerticesLocal"
    PetscErrorCode Viewer::writeVerticesLocal(Obj<ALE::Mesh> mesh, PetscViewer viewer) {
      const Mesh::section_type::patch_type            patch       = 0;
      Obj<Mesh::section_type>                         coordinates = mesh->getSection("coordinates");
      const Obj<Mesh::topology_type>&                 topology    = mesh->getTopologyNew();
      const Obj<Mesh::topology_type::label_sequence>& vertices    = topology->depthStratum(patch, 0);
      int            embedDim    = coordinates->getAtlas()->getFiberDimension(patch, *vertices->begin());
      int            numElements = topology->heightStratum(patch, 0)->size();
      PetscErrorCode ierr;

      PetscFunctionBegin;
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"coord_units = m\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#  Node      X-coord           Y-coord           Z-coord\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);

      for(Mesh::topology_type::label_sequence::iterator v_iter = vertices->begin(); v_iter != vertices->end(); ++v_iter) {
        const Mesh::section_type::value_type *array = coordinates->restrict(patch, *v_iter);

        PetscViewerASCIIPrintf(viewer, "%7D ", (*v_iter).index+1-numElements);
        for(int d = 0; d < embedDim; d++) {
          if (d > 0) {
            PetscViewerASCIIPrintf(viewer, " ");
          }
          PetscViewerASCIIPrintf(viewer, "% 16.8E", array[d]);
        }
        PetscViewerASCIIPrintf(viewer, "\n");
      }
      PetscFunctionReturn(0);
    };
    #undef __FUNCT__  
    #define __FUNCT__ "PyLithWriteElementsLocal"
    PetscErrorCode Viewer::writeElementsLocal(Obj<Mesh> mesh, PetscViewer viewer) {
      const Mesh::topology_type::patch_type           patch    = 0;
      const Obj<Mesh::topology_type>&                 topology = mesh->getTopologyNew();
      const Obj<Mesh::sieve_type>&                    sieve    = topology->getPatch(patch);
      const Obj<Mesh::topology_type::label_sequence>& elements = topology->heightStratum(patch, 0);
      Obj<Mesh::field_type> material;
      int            dim          = mesh->getDimension();
      int            corners      = sieve->nCone(*elements->begin(), topology->depth())->size();
      int            numElements  = elements->size();
      bool           hasMaterial  = false;
      int            elementType  = -1;
      int            elementCount = 1;
      PetscErrorCode ierr;

      PetscFunctionBegin;
      if (dim != 3) {
        SETERRQ(PETSC_ERR_SUP, "PyLith only supports 3D meshes.");
      }
      if (corners == 4) {
        // Linear tetrahedron
        elementType = 5;
      } else if (corners == 8) {
        // Linear hexahedron
        elementType = 1;
      } else {
        SETERRQ1(PETSC_ERR_SUP, "PyLith Error: Unsupported number of elements vertices: %d", corners);
      }
      if (hasMaterial) {
        material = mesh->getField("material");
      }
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#     N ETP MAT INF     N1     N2     N3     N4     N5     N6     N7     N8\n");CHKERRQ(ierr);
      ierr = PetscViewerASCIIPrintf(viewer,"#\n");CHKERRQ(ierr);
      for(Mesh::topology_type::label_sequence::iterator e_iter = elements->begin(); e_iter != elements->end(); ++e_iter) {
        Obj<Mesh::sieve_type::traits::coneSequence> cone = sieve->cone(*e_iter);

        ierr = PetscViewerASCIIPrintf(viewer, "%7d %3d", elementCount++, elementType);CHKERRQ(ierr);
        if (hasMaterial) {
          // No infinite elements
          ierr = PetscViewerASCIIPrintf(viewer, " %3d %3d", (int) material->restrict(Mesh::field_type::patch_type(), *e_iter)[0], 0);CHKERRQ(ierr);
        } else {
          // No infinite elements
          ierr = PetscViewerASCIIPrintf(viewer, " %3d %3d", 1, 0);CHKERRQ(ierr);
        }
        for(Mesh::sieve_type::traits::coneSequence::iterator c_iter = cone->begin(); c_iter != cone->end(); ++c_iter) {
          //FIX: Need a global ordering here
          ierr = PetscViewerASCIIPrintf(viewer, " %6d", (*c_iter).index+1-numElements);CHKERRQ(ierr);
        }
        ierr = PetscViewerASCIIPrintf(viewer, "\n");CHKERRQ(ierr);
      }
      PetscFunctionReturn(0);
    };
    #undef __FUNCT__  
    #define __FUNCT__ "PyLithWriteSplitLocal"
    // The elements seem to be implicitly numbered by appearance, which makes it impossible to
    //   number here by bundle, but we can fix it by traversing the elements like the vertices
    PetscErrorCode Viewer::writeSplitLocal(Obj<ALE::Mesh> mesh, PetscViewer viewer) {
      Obj<ALE::Mesh::sieve_type> topology = mesh->getTopology();
      Obj<ALE::Mesh::field_type> splitField = mesh->getField("split");
      Obj<ALE::Mesh::field_type::order_type::baseSequence> splitElements = splitField->getPatches();
      Obj<ALE::Mesh::bundle_type> elementBundle = mesh->getBundle(topology->depth());
      Obj<ALE::Mesh::bundle_type> vertexBundle = mesh->getBundle(0);
      ALE::Mesh::bundle_type::patch_type patch;
      PetscErrorCode ierr;

      PetscFunctionBegin;
      if (mesh->getDimension() != 3) {
        SETERRQ(PETSC_ERR_SUP, "PyLith only supports 3D meshes.");
      }
      for(ALE::Mesh::field_type::order_type::baseSequence::iterator e_itor = splitElements->begin(); e_itor != splitElements->end(); ++e_itor) {
        const ALE::Mesh::bundle_type::index_type& idx = elementBundle->getIndex(patch, *e_itor);

        if (idx.index > 0) {
          Obj<ALE::Mesh::field_type::order_type::coneSequence> cone = splitField->getPatch(*e_itor);
          int e = idx.prefix+1;

          for(ALE::Mesh::field_type::order_type::coneSequence::iterator c_itor = cone->begin(); c_itor != cone->end(); ++c_itor) {
            const double *values = splitField->restrict(*e_itor, *c_itor);
            int v = vertexBundle->getIndex(patch, *c_itor).prefix+1;

            // No time history
            ierr = PetscViewerASCIIPrintf(viewer, "%6d %6d 0 %15.9g %15.9g %15.9g\n", e, v, values[0], values[1], values[2]);CHKERRQ(ierr);
          }
        }
      }
      PetscFunctionReturn(0);
    };
  };
};
