/**
 * \file createxdmf/main.cpp
 * \brief An utility that generates XDMF files for visualization.
 * \copyright Copyright (c) 2016-2018, Barba group. All rights reserved.
 * \license BSD 3-Clause License.
 * \see createxdmf
 * \ingroup createxdmf
 */


# include <string>
# include <fstream>
# include <sstream>
# include <petscsys.h>
# include <yaml-cpp/yaml.h>
# include <petibm/type.h>
# include <petibm/parser.h>
# include <petibm/mesh.h>


/**
 * \defgroup createxdmf Post-processing utility: createxdmf
 * \brief A post-processing utility that creates XDMF files for visualization.
 * 
 * This is a helper utility built with PetIBM components, and it creates XDMF 
 * files based on the HDF5 files produced by
 * \ref nssolver "Navier-Stokes solver",
 * \ref tairacolonius "IBPM solver", and 
 * \ref decoupledibpm "decoupled IBPM solver".
 * XDMF files can be used for visualization in 
 * [VisIt](https://wci.llnl.gov/simulation/computer-codes/visit/)
 * 
 * If readers are interested in using this utility,
 * please refer to 
 * \ref md_doc_markdowns_runpetibm "Running PetIBM",
 * \ref md_doc_markdowns_examples2d "2D Exmaples", and
 * \ref md_doc_markdowns_examples3d "3D Examples".
 * 
 * \ingroup apps
 */

PetscErrorCode writeSingleXDMF(
        const std::string &directory, const std::string &name, 
        const PetscInt &dim, const petibm::type::IntVec1D &n,
        const PetscInt &bg, const PetscInt &ed, const PetscInt &step);

int main(int argc, char **argv)
{
    
    PetscErrorCode      ierr;
    
    ierr = PetscInitialize(&argc, &argv, nullptr, nullptr); CHKERRQ(ierr);
    
    
    
    YAML::Node  setting;
    
    ierr = petibm::parser::getSettings(setting); CHKERRQ(ierr);
    
    petibm::type::Mesh  mesh;
    
    ierr = petibm::mesh::createMesh(PETSC_COMM_WORLD, setting, mesh); CHKERRQ(ierr);
    
    
    
    PetscInt                    bg;
    PetscInt                    ed;
    PetscInt                    step;
    PetscBool                   isSet;

    // get the range of solutions that are going to be calculated
    ierr = PetscOptionsGetInt(nullptr, nullptr, "-bg", &bg, &isSet); CHKERRQ(ierr);
    if (! isSet) bg = setting["parameters"]["startStep"].as<PetscInt>(0);

    ierr = PetscOptionsGetInt(nullptr, nullptr, "-ed", &ed, &isSet); CHKERRQ(ierr);
    if (! isSet) ed = bg + setting["parameters"]["nt"].as<PetscInt>(); 

    ierr = PetscOptionsGetInt(nullptr, nullptr, "-step", &step, &isSet); CHKERRQ(ierr);
    if (! isSet) step = setting["parameters"]["nsave"].as<PetscInt>(); 
    
    
    
    // u
    ierr = writeSingleXDMF(setting["directory"].as<std::string>(),
            "u", mesh->dim, mesh->n[0], bg, ed, step); CHKERRQ(ierr);
    
    // v
    ierr = writeSingleXDMF(setting["directory"].as<std::string>(),
            "v", mesh->dim, mesh->n[1], bg, ed, step); CHKERRQ(ierr);
    
    // p
    ierr = writeSingleXDMF(setting["directory"].as<std::string>(),
            "p", mesh->dim, mesh->n[3], bg, ed, step); CHKERRQ(ierr);
    
    // wz
    petibm::type::IntVec1D  wn(3);
    
    wn[0] = mesh->n[4][0]; wn[1] = mesh->n[4][1]; wn[2] = mesh->n[3][2];
    ierr = writeSingleXDMF(setting["directory"].as<std::string>(),
            "wz", mesh->dim, wn, bg, ed, step); CHKERRQ(ierr);
    
    if (mesh->dim == 3)
    {
        // w
        ierr = writeSingleXDMF(setting["directory"].as<std::string>(), 
                "w", mesh->dim, mesh->n[2], bg, ed, step); CHKERRQ(ierr);
        
        // wx
        wn[0] = mesh->n[3][0]; wn[1] = mesh->n[4][1]; wn[2] = mesh->n[4][2];
        ierr = writeSingleXDMF(setting["directory"].as<std::string>(),
                "wx", mesh->dim, wn, bg, ed, step); CHKERRQ(ierr);
        
        // wy
        wn[0] = mesh->n[4][0]; wn[1] = mesh->n[3][1]; wn[2] = mesh->n[4][2];
        ierr = writeSingleXDMF(setting["directory"].as<std::string>(),
                "wy", mesh->dim, wn, bg, ed, step); CHKERRQ(ierr);
    }
    
    ierr = PetscFinalize(); CHKERRQ(ierr);
    
    return 0;
} // main


PetscErrorCode writeSingleXDMF(
        const std::string &directory, const std::string &name, 
        const PetscInt &dim, const petibm::type::IntVec1D &n,
        const PetscInt &bg, const PetscInt &ed, const PetscInt &step)
{
    using namespace petibm;
    
    PetscFunctionBeginUser;
    
    PetscErrorCode  ierr;

    PetscViewer         viewer;
    
    std::string         file = directory + "/" + name + ".xmf";
    
    ierr = PetscViewerASCIIOpen(
            PETSC_COMM_WORLD, file.c_str(), &viewer); CHKERRQ(ierr);
    
    // write header
    ierr = PetscViewerASCIIPrintf(viewer, 
            "<?xml version=\'1.0\' encoding=\'ASCII\'?>\n\n"); CHKERRQ(ierr);
    
    
    // write macro definitions
    ierr = PetscViewerASCIIPrintf(viewer, 
            "<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" [\n"); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, 
            "    <!ENTITY CaseDir \"%s\">\n", "./"); CHKERRQ(ierr);
    for(int i=0; i<dim; ++i)
    {
        ierr = PetscViewerASCIIPrintf(viewer, "    <!ENTITY N%s \"%d\">\n", 
                type::dir2str[type::Dir(i)].c_str(), n[i]); CHKERRQ(ierr);
    }
    ierr = PetscViewerASCIIPrintf(viewer, "]>\n\n"); CHKERRQ(ierr);
    
    // write Xdmf block
    ierr = PetscViewerASCIIPrintf(viewer, 
            "<Xdmf Version=\"2.2\">\n"); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, "    "
            "<Information Name=\"MeteData\" Value=\"ID-23454\"/>\n"); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, "    " "<Domain>\n\n"); CHKERRQ(ierr);
    
    // write topology
    ierr = PetscViewerASCIIPrintf(viewer, "    "
            "<Topology Name=\"%s Topo\" ", name.c_str()); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, 
            "TopologyType=\"%dDRectMesh\" ", (dim==3)?3:2); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, 
            "NumberOfElements=\"%s&Ny; &Nx;\"/>\n\n", 
            (dim==3)?"&Nz; ":""); CHKERRQ(ierr);
    
    // write geometry
    ierr = PetscViewerASCIIPrintf(viewer, "    "
            "<Geometry Name=\"%s Geo\" ", name.c_str()); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, 
            "GeometryType=\"VXVY%s\">\n", (dim==3)?"VZ":""); CHKERRQ(ierr);
    
    for(int i=0; i<dim; ++i)
    {
        std::string dir = type::dir2str[type::Dir(i)];
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " 
                "<DataItem Dimensions=\"&N%s;\" ", dir.c_str()); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, 
                "Format=\"HDF\" NumberType=\"Float\" Precision=\"8\">\n"); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    " 
                "&CaseDir;/grid.h5:/%s/%s\n", name.c_str(), dir.c_str()); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " 
                "</DataItem>\n"); CHKERRQ(ierr);
    }
    
    ierr = PetscViewerASCIIPrintf(viewer, "    " "</Geometry>\n\n"); CHKERRQ(ierr);
    
    
    //write temporal grid collection
    ierr = PetscViewerASCIIPrintf(viewer, "    "
            "<Grid GridType=\"Collection\" CollectionType=\"Temporal\">\n\n"); CHKERRQ(ierr);
    
    // write each step
    for(PetscInt t=bg; t<=ed; t+=step)
    {
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " 
                "<Grid GridType=\"Uniform\" Name=\"%s Grid\">\n", name.c_str()); CHKERRQ(ierr);
        
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    "
                "<Time Value=\"%07d\" />\n", t); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    "
                "<Topology Reference=\"/Xdmf/Domain/Topology"
                "[@Name=\'%s Topo\']\" />\n", name.c_str()); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    "
                "<Geometry Reference=\"/Xdmf/Domain/Geometry"
                "[@Name=\'%s Geo\']\" />\n", name.c_str()); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    "
                "<Attribute Name=\"%s\" AttributeType=\"Scalar\" Center=\"Node\">\n",
                name.c_str()); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    " "    "
                "<DataItem Dimensions=\"%s&Ny; &Nx;\" ",
                (dim==3)?"&Nz; ":""); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer,
                "Format=\"HDF\" NumberType=\"Float\" Precision=\"8\">\n"); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    " "    " "    "
                "&CaseDir;/solution/%07d.h5:/%s\n", t, name.c_str()); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    " "    "
                "</DataItem>\n"); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "    "
                "</Attribute>\n"); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPrintf(viewer, "    " "    " "</Grid>\n\n"); CHKERRQ(ierr);
    }
    
    ierr = PetscViewerASCIIPrintf(viewer, "    "
            "</Grid>\n\n"); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, "    " "</Domain>\n"); CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer, "</Xdmf>\n"); CHKERRQ(ierr);
    
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
    
    PetscFunctionReturn(0);
} // writeSingleXDMF
