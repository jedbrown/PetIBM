/**
 * \file creategradient.cpp
 * \brief Definition of functions creating gradient operator.
 * \copyright Copyright (c) 2016-2018, Barba group. All rights reserved.
 * \license BSD 3-Clause License.
 */


//
# include <functional>

// here goes PETSc headers
# include <petscmat.h>

// here goes headers from our PetIBM
# include <petibm/mesh.h>
# include <petibm/type.h>


namespace petibm
{
namespace operators
{

/** \brief STL vector holding MatStencil. */
typedef std::vector<MatStencil> StencilVec;


/** \brief a function type for functions returning neighbors' stencils. */
typedef std::function<StencilVec(
        const PetscInt &, const PetscInt &, const PetscInt &)> GetNeighborFunc;


/** \brief a function type for functions computing matrix entries' values. */
typedef std::function<type::RealVec1D(
        const PetscInt &, const PetscInt &, const PetscInt &)> Kernel;


// implementation of petibm::operators::createGradient
PetscErrorCode createGradient(const type::Mesh &mesh,
                              Mat &G,
                              const PetscBool &normalize)
{
    PetscFunctionBeginUser;

    PetscErrorCode                  ierr;
    
    std::vector<GetNeighborFunc>    getNeighbor(3);

    std::vector<Kernel>             kernel(3);


    // set up the function used to get the IDs of neighbors
    getNeighbor[0] = [](const PetscInt &i, const PetscInt &j, const PetscInt &k)
        -> std::vector<MatStencil> { return {{k, j, i, 0}, {k, j, i+1, 0}}; };

    getNeighbor[1] = [](const PetscInt &i, const PetscInt &j, const PetscInt &k)
        -> std::vector<MatStencil> { return {{k, j, i, 0}, {k, j+1, i, 0}}; };

    getNeighbor[2] = [](const PetscInt &i, const PetscInt &j, const PetscInt &k)
        -> std::vector<MatStencil> { return {{k, j, i, 0}, {k+1, j, i, 0}}; };


    // set up the kernel that calculates the entry values
    if (normalize)
        kernel[0] = kernel[1] = kernel[2] =
            std::bind([]() -> type::RealVec1D { return {-1.0, 1.0}; });
    else
    {
        kernel[0] =
            [&mesh](const PetscInt &i, const PetscInt &j, const PetscInt &k)
            -> type::RealVec1D {PetscReal v=1.0/mesh->dL[0][0][i]; return {-v, v};};

        kernel[1] = 
            [&mesh](const PetscInt &i, const PetscInt &j, const PetscInt &k)
            -> type::RealVec1D {PetscReal v=1.0/mesh->dL[1][1][j]; return {-v, v};};

        kernel[2] =
            [&mesh](const PetscInt &i, const PetscInt &j, const PetscInt &k)
            -> type::RealVec1D {PetscReal v=1.0/mesh->dL[2][2][k]; return {-v, v};};
    }


    // create matrix
    ierr = MatCreate(mesh->comm, &G); CHKERRQ(ierr);
    ierr = MatSetSizes(G, mesh->UNLocal, mesh->pNLocal,
            PETSC_DETERMINE, PETSC_DETERMINE); CHKERRQ(ierr);
    ierr = MatSetFromOptions(G); CHKERRQ(ierr);
    ierr = MatSeqAIJSetPreallocation(G, 2, nullptr); CHKERRQ(ierr);
    ierr = MatMPIAIJSetPreallocation(G, 2, nullptr, 1, nullptr); CHKERRQ(ierr);
    ierr = MatSetUp(G); CHKERRQ(ierr);
    ierr = MatSetOption(G, MAT_KEEP_NONZERO_PATTERN, PETSC_FALSE); CHKERRQ(ierr);
    ierr = MatSetOption(G, MAT_IGNORE_ZERO_ENTRIES, PETSC_TRUE); CHKERRQ(ierr);


    // set values to matrix
    for(PetscInt field=0; field<mesh->dim; ++field)
        for(PetscInt k=mesh->bg[field][2]; k<mesh->ed[field][2]; ++k)
            for(PetscInt j=mesh->bg[field][1]; j<mesh->ed[field][1]; ++j)
                for(PetscInt i=mesh->bg[field][0]; i<mesh->ed[field][0]; ++i)
                {
                    PetscInt            rId, cId;

                    StencilVec          loc = getNeighbor[field](i, j, k);

                    type::RealVec1D     values = kernel[field](i, j, k);

                    // get packed index of this velocity point
                    ierr = mesh->getPackedGlobalIndex(
                            field, loc[0], rId); CHKERRQ(ierr);

                    // loop through columns
                    for(PetscInt n=0; n<2; ++n)
                    {
                        ierr = mesh->getPackedGlobalIndex(
                                3, loc[n], cId); CHKERRQ(ierr);

                        ierr = MatSetValue(G, rId, cId, 
                                values[n], INSERT_VALUES); CHKERRQ(ierr);
                    }
                }


    // assemble matrix
    ierr = MatAssemblyBegin(G, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
    ierr = MatAssemblyEnd(G, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

    PetscFunctionReturn(0);
} // createGradient

} // end of namespace operators
} // end of namespace petibm
