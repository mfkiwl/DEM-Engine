#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <memory>
#include "API.h"
#include "AuxClasses.h"
#include <core/utils/DEMEPaths.h>
#include "VariableTypes.h"
#include <vector>
#include <string>
#include <limits>
#include <iostream>
#include <sstream>
#include <array>
#include <cmath>
#include <string>

#include <core/utils/RuntimeData.h>
#include <nvmath/helper_math.cuh>
#include <DEM/Defines.h>
#include <DEM/Structs.h>
#include <core/utils/ManagedAllocator.hpp>
#include <DEM/HostSideHelpers.hpp>
#include <DEM/utils/Samplers.hpp>

namespace py = pybind11;

PYBIND11_MODULE(DEME, obj) {
    // Obtaining the location of python's site-packages dynamically and setting it
    // as path prefix

    py::module _site = py::module_::import("site");
    std::string loc = _site.attr("getsitepackages")().cast<py::list>()[0].cast<py::str>();

    // Setting path prefix
    std::filesystem::path path = loc;
    RuntimeDataHelper::SetPathPrefix(path);
    deme::SetDEMEDataPath(path / "share/data");

    // Setting JitHelper variables
    deme::SetDEMEKernelPath(path / "share/kernel");
    deme::SetDEMEIncludePath(path / "include");

    // To define methods independent of a class, use obj.def() syntax to wrap them!
    obj.def("FrameTransformGlobalToLocal", &deme::FrameTransformGlobalToLocal,
            "Translating the inverse of the provided vec then applying a local inverse rotation of the provided rot_Q, "
            "then return the result.");
    obj.def("FrameTransformLocalToGlobal", &deme::FrameTransformLocalToGlobal,
            "Apply a local rotation then a translation, then return the result.");
    obj.def("GetDEMEDataFile", &deme::GetDEMEDataFile);
    obj.def("DEMBoxGridSampler",
            static_cast<std::vector<std::vector<float>> (*)(const std::vector<float>&, const std::vector<float>&, float,
                                                            float, float)>(&deme::DEMBoxGridSampler),
            py::arg("BoxCenter"), py::arg("HalfDims"), py::arg("GridSizeX"), py::arg("GridSizeY") = -1.0,
            py::arg("GridSizeZ") = -1.0);
    obj.def(
        "DEMBoxHCPSampler",
        static_cast<std::vector<std::vector<float>> (*)(const std::vector<float>&, const std::vector<float>&, float)>(
            &deme::DEMBoxHCPSampler));
    obj.def("DEMCylSurfSampler",
            static_cast<std::vector<std::vector<float>> (*)(const std::vector<float>&, const std::vector<float>&, float,
                                                            float, float, float)>(&deme::DEMCylSurfSampler),
            py::arg("CylCenter"), py::arg("CylAxis"), py::arg("CylRad"), py::arg("CylHeight"), py::arg("ParticleRad"),
            py::arg("spacing") = 1.2);

    obj.attr("PI") = py::float_(3.141592653589793238462643383279502884197);

    py::class_<RuntimeDataHelper>(obj, "RuntimeDataHelper")
        .def(py::init<>())
        .def_static("SetPathPrefix", &RuntimeDataHelper::SetPathPrefix);

    py::class_<deme::PDSampler>(obj, "PDSampler")
        .def(py::init<float>())
        .def("SetSeparation", &deme::PDSampler::SetSeparation)
        .def("SampleBox",
             static_cast<std::vector<std::vector<float>> (deme::PDSampler::*)(
                 const std::vector<float>& center, const std::vector<float>& halfDim)>(&deme::PDSampler::SampleBox),
             "Generates a sample box")

        .def("SampleSphere",
             static_cast<std::vector<std::vector<float>> (deme::PDSampler::*)(const std::vector<float>&, float)>(
                 &deme::PDSampler::SampleSphere))

        .def("SampleCylinderX",
             static_cast<std::vector<std::vector<float>> (deme::PDSampler::*)(const std::vector<float>&, float, float)>(
                 &deme::PDSampler::SampleCylinderX))

        .def("SampleCylinderY",
             static_cast<std::vector<std::vector<float>> (deme::PDSampler::*)(const std::vector<float>&, float, float)>(
                 &deme::PDSampler::SampleCylinderY))

        .def("SampleCylinderZ",
             static_cast<std::vector<std::vector<float>> (deme::PDSampler::*)(const std::vector<float>&, float, float)>(
                 &deme::PDSampler::SampleCylinderZ))

        .def("GetSeparation", &deme::Sampler::GetSeparation);

    py::class_<deme::GridSampler>(obj, "GridSampler")
        .def(py::init<float>())
        .def("SetSeparation", &deme::GridSampler::SetSeparation)
        .def("SampleBox",
             static_cast<std::vector<std::vector<float>> (deme::GridSampler::*)(
                 const std::vector<float>& center, const std::vector<float>& halfDim)>(&deme::GridSampler::SampleBox),
             "Generates a sample box")

        .def("SampleSphere",
             static_cast<std::vector<std::vector<float>> (deme::GridSampler::*)(const std::vector<float>&, float)>(
                 &deme::GridSampler::SampleSphere))

        .def("SampleCylinderX", static_cast<std::vector<std::vector<float>> (deme::GridSampler::*)(
                                    const std::vector<float>&, float, float)>(&deme::GridSampler::SampleCylinderX))

        .def("SampleCylinderY", static_cast<std::vector<std::vector<float>> (deme::GridSampler::*)(
                                    const std::vector<float>&, float, float)>(&deme::GridSampler::SampleCylinderY))

        .def("SampleCylinderZ", static_cast<std::vector<std::vector<float>> (deme::GridSampler::*)(
                                    const std::vector<float>&, float, float)>(&deme::GridSampler::SampleCylinderZ))

        .def("GetSeparation", &deme::Sampler::GetSeparation);

    py::class_<deme::HCPSampler>(obj, "HCPSampler")
        .def(py::init<float>())
        .def("SampleBox",
             static_cast<std::vector<std::vector<float>> (deme::HCPSampler::*)(
                 const std::vector<float>& center, const std::vector<float>& halfDim)>(&deme::HCPSampler::SampleBox),
             "Generates a sample box")

        .def("SetSeparation", &deme::HCPSampler::SetSeparation)
        .def("SampleSphere",
             static_cast<std::vector<std::vector<float>> (deme::HCPSampler::*)(const std::vector<float>&, float)>(
                 &deme::HCPSampler::SampleSphere))

        .def(
            "SampleCylinderX",
            static_cast<std::vector<std::vector<float>> (deme::HCPSampler::*)(const std::vector<float>&, float, float)>(
                &deme::HCPSampler::SampleCylinderX))

        .def(
            "SampleCylinderY",
            static_cast<std::vector<std::vector<float>> (deme::HCPSampler::*)(const std::vector<float>&, float, float)>(
                &deme::HCPSampler::SampleCylinderY))

        .def(
            "SampleCylinderZ",
            static_cast<std::vector<std::vector<float>> (deme::HCPSampler::*)(const std::vector<float>&, float, float)>(
                &deme::HCPSampler::SampleCylinderZ))

        .def("GetSeparation", &deme::HCPSampler::GetSeparation);

    py::class_<deme::DEMInspector, std::shared_ptr<deme::DEMInspector>>(obj, "DEMInspector")
        .def(py::init<deme::DEMSolver*, deme::DEMDynamicThread*, const std::string&>())
        .def("GetValue", &deme::DEMInspector::GetValue);

    py::class_<deme::DEMInitializer, std::shared_ptr<deme::DEMInitializer>>(obj, "DEMInitializer").def(py::init<>());

    py::class_<deme::DEMTrackedObj, deme::DEMInitializer, std::shared_ptr<deme::DEMTrackedObj>>(obj, "DEMTrackedObj")
        .def(py::init<deme::DEMTrackedObj>());

    py::class_<deme::DEMTracker, std::shared_ptr<deme::DEMTracker>>(obj, "Tracker")
        .def(py::init<deme::DEMSolver*>())
        .def("GetContactForcesAndLocalTorque",
             static_cast<std::vector<std::vector<std::vector<float>>> (deme::DEMTracker::*)(size_t)>(
                 &deme::DEMTracker::GetContactForcesAndLocalTorque),
             "Get all contact forces and local torques that concern this track object, as a list.",
             py::arg("offset") = 0)
        .def("GetContactForcesAndGlobalTorque",
             static_cast<std::vector<std::vector<std::vector<float>>> (deme::DEMTracker::*)(size_t)>(
                 &deme::DEMTracker::GetContactForcesAndGlobalTorque),
             "Get all contact forces and global torques that concern this track object, as a list.",
             py::arg("offset") = 0)
        .def("GetContactForces",
             static_cast<std::vector<std::vector<std::vector<float>>> (deme::DEMTracker::*)(size_t)>(
                 &deme::DEMTracker::GetContactForces),
             "Get all contact forces that concern this track object, as a list.", py::arg("offset") = 0)
        .def("GetOwnerID", &deme::DEMTracker::GetOwnerID, "Get the owner ID of the tracked obj.", py::arg("offset") = 0)

        .def("Pos", &deme::DEMTracker::GetPos, "Get the position of this tracked object.", py::arg("offset") = 0)
        .def("AngVelLocal", &deme::DEMTracker::GetAngVelLocal,
             "Get the angular velocity of this tracked object in its own local coordinate system. Applying OriQ to it "
             "would give you the ang vel in global frame.",
             py::arg("offset") = 0)
        .def("AngVelGlobal", &deme::DEMTracker::GetAngVelGlobal,
             "Get the angular velocity of this tracked object in global coordinate system.", py::arg("offset") = 0)
        .def("Vel", &deme::DEMTracker::GetVel, "Get the velocity of this tracked object in global frame.",
             py::arg("offset") = 0)
        .def("OriQ", &deme::DEMTracker::GetOriQ,
             "Get the quaternion that represents the orientation of this tracked object's own coordinate system.",
             py::arg("offset") = 0)
        .def("MOI", &deme::DEMTracker::GetMOI,
             "Get the moment of inertia (in principal axis frame) of the tracked object.", py::arg("offset") = 0)
        .def("Mass", &deme::DEMTracker::Mass, "Get the mass of the tracked object.", py::arg("offset") = 0)
        .def("GetFamily", &deme::DEMTracker::GetFamily, "Get the family number of the tracked object.",
             py::arg("offset") = 0)
        .def("GetContactClumps", &deme::DEMTracker::GetContactClumps,
             "Get the clumps that are in contact with this tracked owner as a vector.", py::arg("offset") = 0)
        .def("ContactAcc", &deme::DEMTracker::GetContactAcc,
             "Get the a portion of the acceleration of this tracked object, that is the result of its contact with "
             "other simulation entities. In most cases, this means excluding the gravitational acceleration. The "
             "acceleration is in global frame.",
             py::arg("offset") = 0)
        .def("ContactAngAccLocal", &deme::DEMTracker::GetContactAngAccLocal,
             "Get the a portion of the angular acceleration of this tracked object, that is the result of its contact "
             "with other simulation entities. The acceleration is in this object's local frame.",
             py::arg("offset") = 0)
        .def("ContactAngAccGlobal", &deme::DEMTracker::GetContactAngAccGlobal,
             "Get the a portion of the angular acceleration of this tracked object, that is the result of its contact "
             "with other simulation entities. The acceleration is in this object's global frame.",
             py::arg("offset") = 0)

        .def("GetOwnerWildcardValue",
             static_cast<float (deme::DEMTracker::*)(const std::string&, size_t)>(
                 &deme::DEMTracker::GetOwnerWildcardValue),
             "Get the owner's wildcard value.", py::arg("name"), py::arg("offset") = 0)
        .def("GetGeometryWildcardValues",
             static_cast<std::vector<float> (deme::DEMTracker::*)(const std::string&)>(
                 &deme::DEMTracker::GetGeometryWildcardValues),
             "Get the geometry wildcard values for all the geometry entities tracked by this tracker.", py::arg("name"))
        .def("GetGeometryWildcardValue",
             static_cast<float (deme::DEMTracker::*)(const std::string&, size_t)>(
                 &deme::DEMTracker::GetGeometryWildcardValue),
             "Get the geometry wildcard values for a geometry entity tracked by this tracker.", py::arg("name"),
             py::arg("offset"))

        .def("SetPos",
             static_cast<void (deme::DEMTracker::*)(const std::vector<float>&, size_t)>(&deme::DEMTracker::SetPos),
             "Set the position of this tracked object.", py::arg("pos"), py::arg("offset") = 0)
        .def("SetAngVel",
             static_cast<void (deme::DEMTracker::*)(const std::vector<float>&, size_t)>(&deme::DEMTracker::SetAngVel),
             "Set the angular velocity of this tracked object in its own local coordinate system.", py::arg("angVel"),
             py::arg("offset") = 0)
        .def("SetVel",
             static_cast<void (deme::DEMTracker::*)(const std::vector<float>&, size_t)>(&deme::DEMTracker::SetVel),
             "Set the velocity of this tracked object in global frame.", py::arg("vel"), py::arg("offset") = 0)
        .def("SetOriQ",
             static_cast<void (deme::DEMTracker::*)(const std::vector<float>&, size_t)>(&deme::DEMTracker::SetOriQ),
             "Set the quaternion which represents the orientation of this tracked object's coordinate system.",
             py::arg("oriQ"), py::arg("offset") = 0)
        .def("AddAcc",
             static_cast<void (deme::DEMTracker::*)(const std::vector<float>&, size_t)>(&deme::DEMTracker::AddAcc),
             "Add an extra acc to the tracked body, for the next time step. Note if the user intends to add a "
             "persistent external force, then using family prescription is the better method.",
             py::arg("acc"), py::arg("offset") = 0)
        .def("AddAngAcc",
             static_cast<void (deme::DEMTracker::*)(const std::vector<float>&, size_t)>(&deme::DEMTracker::AddAngAcc),
             "Add an extra angular acceleration to the tracked body, for the next time step. Note if the user intends "
             "to add a persistent external torque, then using family prescription is the better method.",
             py::arg("angAcc"), py::arg("offset") = 0)

        .def("SetFamily", static_cast<void (deme::DEMTracker::*)(unsigned int)>(&deme::DEMTracker::SetFamily),
             "Change the family numbers of all the entities tracked by this tracker.", py::arg("fam_num"))
        .def("SetFamily",
             static_cast<void (deme::DEMTracker::*)(const std::vector<unsigned int>&)>(&deme::DEMTracker::SetFamily),
             "Change the family numbers of all the entities tracked by this tracker by supplying a list the same "
             "length as the number of tracked entities.",
             py::arg("fam_nums"))
        .def("SetFamily", static_cast<void (deme::DEMTracker::*)(unsigned int, size_t)>(&deme::DEMTracker::SetFamily),
             "Change the family number of one entities tracked by this tracker.", py::arg("fam_num"), py::arg("offset"))

        .def("UpdateMesh",
             static_cast<void (deme::DEMTracker::*)(const std::vector<std::vector<float>>&)>(
                 &deme::DEMTracker::UpdateMesh),
             "Apply the new mesh node positions such that the tracked mesh is replaced by the new_nodes.",
             py::arg("new_nodes"))
        .def("UpdateMeshByIncrement",
             static_cast<void (deme::DEMTracker::*)(const std::vector<std::vector<float>>&)>(
                 &deme::DEMTracker::UpdateMeshByIncrement),
             "Change the coordinates of each mesh node by the given amount.", py::arg("deformation"))

        .def("GetMeshNodesGlobal",
             static_cast<std::vector<std::vector<float>> (deme::DEMTracker::*)()>(
                 &deme::DEMTracker::GetMeshNodesGlobalAsVectorOfVector),
             "Get the current locations of all the nodes in the mesh being tracked.")

        .def("GetMesh", &deme::DEMTracker::GetMesh, "Get a handle for the mesh this tracker is tracking.")

        .def("SetOwnerWildcardValue", &deme::DEMTracker::SetOwnerWildcardValue,
             "Set a wildcard value of the owner this tracker is tracking.", py::arg("name"), py::arg("wc"),
             py::arg("offset") = 0)
        .def("SetOwnerWildcardValues", &deme::DEMTracker::SetOwnerWildcardValues,
             "Set a wildcard value of the owner this tracker is tracking.", py::arg("name"), py::arg("wc"))
        .def("SetGeometryWildcardValue", &deme::DEMTracker::SetGeometryWildcardValue,
             "Set a wildcard value of the geometry entity this tracker is tracking.", py::arg("name"), py::arg("wc"),
             py::arg("offset") = 0)
        .def("SetGeometryWildcardValues", &deme::DEMTracker::SetGeometryWildcardValues,
             "Set a wildcard value of the geometry entities this tracker is tracking.", py::arg("name"), py::arg("wc"));

    py::class_<deme::DEMForceModel, std::shared_ptr<deme::DEMForceModel>>(obj, "DEMForceModel")
        .def(py::init<deme::FORCE_MODEL>())
        .def("SetForceModelType", &deme::DEMForceModel::SetForceModelType, "Set the contact force model type")
        .def("DefineCustomModel", &deme::DEMForceModel::DefineCustomModel,
             "Define user-custom force model with a string which is your force calculation code")
        .def("ReadCustomModelFile", &deme::DEMForceModel::ReadCustomModelFile,
             "Read user-custom force model from a file (which by default should reside in kernel/DEMUserScripts), "
             "which contains your force calculation code. Returns 0 if read successfully, otherwise 1.")
        .def("SetMustHaveMatProp", &deme::DEMForceModel::SetMustHaveMatProp,
             "Specifiy the material properties that this force model will use")
        .def("SetMustPairwiseMatProp", &deme::DEMForceModel::SetMustPairwiseMatProp,
             "Specifiy the material properties that are pair-wise (instead of being associated with each individual "
             "material).")
        .def("SetPerContactWildcards", &deme::DEMForceModel::SetPerContactWildcards,
             "Set the names for the extra quantities that will be associated with each contact pair. For example, "
             "history-based models should have 3 float arrays to store contact history. Only float is supported. Note "
             "the initial value of all contact wildcard arrays is automatically 0")
        .def("SetPerOwnerWildcards", &deme::DEMForceModel::SetPerOwnerWildcards,
             " Set the names for the extra quantities that will be associated with each owner. For example, you can "
             "use this to associate a cohesion parameter to each particle. Only float is supported.")
        .def("SetPerGeometryWildcards", &deme::DEMForceModel::SetPerGeometryWildcards,
             "Set the names for the extra quantities that will be associated with each geometry. For example, you can "
             "use this to associate certain electric charges to each particle's each component which represents a "
             "distribution of the charges. Only float is supported.");

    py::class_<deme::DEMSolver>(obj, "DEMSolver")
        .def(py::init<unsigned int>(), py::arg("nGPUs") = 2)
        .def("UpdateStepSize", &deme::DEMSolver::UpdateStepSize,
             "Update the time step size. Used after system initialization.", py::arg("ts") = -1.0)
        .def("SetNoForceRecord", &deme::DEMSolver::SetNoForceRecord,
             "Instruct the solver that there is no need to record the contact force (and contact point location etc.) "
             "in an array.",
             py::arg("flag") = true)
        .def("LoadSphereType", &deme::DEMSolver::LoadSphereType)
        .def("EnsureKernelErrMsgLineNum", &deme::DEMSolver::EnsureKernelErrMsgLineNum,
             "If true, each jitification string substitution will do a one-liner to one-liner replacement, so that if "
             "the kernel compilation fails, the error meessage line number will reflex the actual spot where that "
             "happens (instead of some random number).",
             py::arg("flag") = true)
        .def("UseCubForceCollection", &deme::DEMSolver::UseCubForceCollection,
             "Whether the force collection (acceleration calc and reduction) process should be using CUB. If true, the "
             "acceleration array is flattened and reduced using CUB; if false, the acceleration is computed and "
             "directly applied to each body through atomic operations.",
             py::arg("flag") = true)
        .def("SetCollectAccRightAfterForceCalc", &deme::DEMSolver::SetCollectAccRightAfterForceCalc,
             "Reduce contact forces to accelerations right after calculating them, in the same kernel. This may give "
             "some performance boost if you have only polydisperse spheres, no clumps.",
             py::arg("flag") = true)

        .def("SetInitBinSize", &deme::DEMSolver::SetInitBinSize,
             " Explicitly instruct the bin size (for contact detection) that the solver should use.")
        .def("SetOutputFormat",
             static_cast<void (deme::DEMSolver::*)(const std::string&)>(&deme::DEMSolver::SetOutputFormat),
             "Choose sphere and clump output file format.")
        .def("GetNumContacts", &deme::DEMSolver::GetNumContacts,
             "Get the number of kT-reported potential contact pairs.")
        .def("GetTimeStepSize", &deme::DEMSolver::GetTimeStepSize, "Get the current time step size in simulation.")
        .def("SetCDUpdateFreq", &deme::DEMSolver::SetCDUpdateFreq,
             "Set the number of dT steps before it waits for a contact-pair info update from kT.")
        .def("GetSimTime", &deme::DEMSolver::GetSimTime,
             "Get the simulation time passed since the start of simulation.")
        .def("SetSimTime", &deme::DEMSolver::SetSimTime,
             "Get the simulation time passed since the start of simulation.")
        .def("UpdateClumps", &deme::DEMSolver::UpdateClumps,
             "Transfer newly loaded clumps to the GPU-side in mid-simulation.")
        .def("SetAdaptiveTimeStepType", &deme::DEMSolver::SetAdaptiveTimeStepType,
             "Set the strategy for auto-adapting time step size (NOT implemented, no effect yet).")
        .def("SetIntegrator",
             static_cast<void (deme::DEMSolver::*)(const std::string&)>(&deme::DEMSolver::SetIntegrator),
             "Set the time integrator for this simulator.")
        .def("SetIntegrator",
             static_cast<void (deme::DEMSolver::*)(deme::TIME_INTEGRATOR)>(&deme::DEMSolver::SetIntegrator),
             "Set the time integrator for this simulator.")
        .def("GetInitStatus", &deme::DEMSolver::GetInitStatus, "Return whether this simulation system is initialized.")
        .def("GetJitStringSubs", &deme::DEMSolver::GetJitStringSubs,
             "Get the jitification string substitution laundary list. It is needed by some of this simulation system's "
             "friend classes.")

        .def("SetInitBinSizeAsMultipleOfSmallestSphere", &deme::DEMSolver::SetInitBinSizeAsMultipleOfSmallestSphere,
             "Explicitly instruct the bin size (for contact detection) that the solver should use, as a multiple of "
             "the radius of the smallest sphere in simulation.")
        .def(
            "SetInitBinNumTarget", &deme::DEMSolver::SetInitBinNumTarget,
            "Set the target number of bins (for contact detection) at the start of the simulation upon initialization.")
        .def("InstructNumOwners", &deme::DEMSolver::InstructNumOwners,
             "Explicitly instruct the sizes for the arrays at initialization time. This is useful when the number of "
             "owners tends to change (especially gradually increase) frequently in the simulation.")
        .def("UseFrictionalHertzianModel", &deme::DEMSolver::UseFrictionalHertzianModel,
             py::return_value_policy::reference_internal,
             "Instruct the solver to use frictonal (history-based) Hertzian contact force model.")
        .def("UseFrictionlessHertzianModel", &deme::DEMSolver::UseFrictionlessHertzianModel,
             py::return_value_policy::reference_internal,
             "Instruct the solver to use frictonless Hertzian contact force model")
        .def("DefineContactForceModel", &deme::DEMSolver::DefineContactForceModel,
             py::return_value_policy::reference_internal,
             "Define a custom contact force model by a string. Returns a pointer to the force model in use.")
        .def("ReadContactForceModel", &deme::DEMSolver::ReadContactForceModel,
             py::return_value_policy::reference_internal,
             "Read user custom contact force model from a file (which by default should reside in "
             "kernel/DEMUserScripts). Returns a pointer to the force model in use.")

        .def("GetContactForceModel", &deme::DEMSolver::GetContactForceModel, "Get the current force model",
             py::return_value_policy::reference_internal)
        .def("SetSortContactPairs", &deme::DEMSolver::SetSortContactPairs,
             "Instruct the solver if contact pair arrays should be sorted (based on the types of contacts) before "
             "usage.")
        .def(
            "SetJitifyClumpTemplates", &deme::DEMSolver::SetJitifyClumpTemplates,
            "Instruct the solver to rearrange and consolidate clump templates information, then jitify it into GPU "
            "kernels (if set to true), rather than using flattened sphere component configuration arrays whose entries "
            "are associated with individual spheres.",
            py::arg("use") = true)
        .def("SetJitifyMassProperties", &deme::DEMSolver::SetJitifyMassProperties,
             "Instruct the solver to rearrange and consolidate mass property information (for all owner types), then "
             "jitify it into GPU kernels (if set to true), rather than using flattened mass property arrays whose "
             "entries are associated with individual owners.",
             py::arg("use") = true)
        .def("SetExpandFactor", &deme::DEMSolver::SetExpandFactor,
             "(Explicitly) set the amount by which the radii of the spheres (and the thickness of the boundaries) are "
             "expanded for the purpose of contact detection (safe, and creates false positives). If fix is set to "
             "true, then this expand factor does not change even if the user uses variable time step size.",
             py::arg("beta"), py::arg("fix") = true)

        .def("SetExpandSafetyType", &deme::DEMSolver::SetExpandSafetyType,
             "A string. If 'auto': the solver automatically derives.")
        .def("SetExpandSafetyAdder", &deme::DEMSolver::SetExpandSafetyAdder,
             "Set a `base' velocity, which we will always add to our estimated maximum system velocity, when deriving "
             "the thinckness of the contact `safety' margin")
        .def("SetMaxSphereInBin", &deme::DEMSolver::SetMaxSphereInBin,
             "Used to force the solver to error out when there are too many spheres in a bin. A huge number can be "
             "used to discourage this error type")
        .def("SetMaxTriangleInBin", &deme::DEMSolver::SetMaxTriangleInBin,
             "Used to force the solver to error out when there are too many spheres in a bin. A huge number can be "
             "used to discourage this error type")

        .def("SetErrorOutAvgContacts", &deme::DEMSolver::SetErrorOutAvgContacts,
             "Set the average number of contacts a sphere has, before the solver errors out. A huge number can be used "
             "to discourage this error type. Defaulted to 100")
        .def("GetAvgSphContacts", &deme::DEMSolver::GetAvgSphContacts,
             "Get the current number of contacts each sphere has")
        .def("UseAdaptiveBinSize", &deme::DEMSolver::UseAdaptiveBinSize,
             "Enable or disable the use of adaptive bin size (by default it is on)", py::arg("use") = true)
        .def("DisableAdaptiveBinSize", &deme::DEMSolver::DisableAdaptiveBinSize,
             "Disable the use of adaptive bin size (always use initial size)")
        .def("UseAdaptiveUpdateFreq", &deme::DEMSolver::UseAdaptiveUpdateFreq,
             "Enable or disable the use of adaptive max update step count (by default it is on)", py::arg("use") = true)
        .def("DisableAdaptiveUpdateFreq", &deme::DEMSolver::DisableAdaptiveUpdateFreq,
             "Disable the use of adaptive max update step count (always use initial update frequency)")
        .def("SetAdaptiveBinSizeDelaySteps", &deme::DEMSolver::SetAdaptiveBinSizeDelaySteps,
             "Adjust how frequent kT updates the bin size")
        .def("SetAdaptiveBinSizeMaxRate", &deme::DEMSolver::SetAdaptiveBinSizeMaxRate,
             "Set the max rate that the bin size can change in one adjustment")
        .def("SetAdaptiveBinSizeAcc", &deme::DEMSolver::SetAdaptiveBinSizeAcc,
             "Set how fast kT changes the direction of bin size adjustmemt when there's a more beneficial direction")
        .def("SetAdaptiveBinSizeUpperProactivity", &deme::DEMSolver::SetAdaptiveBinSizeUpperProactivity,
             "Set how proactive the solver is in avoiding the bin being too big (leading to too many geometries in a "
             "bin)")
        .def(
            "SetAdaptiveBinSizeLowerProactivity", &deme::DEMSolver::SetAdaptiveBinSizeLowerProactivity,
            "Set how proactive the solver is in avoiding the bin being too small (leading to too many bins in domain).")
        .def("GetBinSize", &deme::DEMSolver::GetBinSize,
             "Get the current bin (for contact detection) size. Must be called from synchronized stance.")
        .def("GetBinNum", &deme::DEMSolver::GetBinNum,
             "Get the current number of bins (for contact detection). Must be called from synchronized stance.")
        .def("SetCDMaxUpdateFreq", &deme::DEMSolver::SetCDMaxUpdateFreq,
             "Set the upper bound of kT update frequency (when it is adjusted automatically).")
        .def("SetCDNumStepsMaxDriftAheadOfAvg", &deme::DEMSolver::SetCDNumStepsMaxDriftAheadOfAvg,
             "Set the number of steps dT configures its max drift more than average drift steps.")
        .def("SetCDNumStepsMaxDriftMultipleOfAvg", &deme::DEMSolver::SetCDNumStepsMaxDriftMultipleOfAvg,
             "Set the multiplier which dT configures its max drift to be w.r.t. the average drift steps.")
        .def("SetCDNumStepsMaxDriftHistorySize", &deme::DEMSolver::SetCDNumStepsMaxDriftHistorySize)
        .def("GetUpdateFreq", &deme::DEMSolver::GetUpdateFreq, "Get the current update frequency used by the solver.")
        .def("SetForceCalcThreadsPerBlock", &deme::DEMSolver::SetForceCalcThreadsPerBlock,
             "Set the number of threads per block in force calculation (default 256).")
        .def("Duplicate",
             static_cast<std::shared_ptr<deme::DEMMaterial> (deme::DEMSolver::*)(
                 const std::shared_ptr<deme::DEMMaterial>&)>(&deme::DEMSolver::Duplicate),
             "Duplicate a material that is loaded into the system.")
        .def("Duplicate",
             static_cast<std::shared_ptr<deme::DEMClumpTemplate> (deme::DEMSolver::*)(
                 const std::shared_ptr<deme::DEMClumpTemplate>&)>(&deme::DEMSolver::Duplicate),
             "Duplicate a clump template that is loaded into the system.")
        .def("Duplicate",
             static_cast<std::shared_ptr<deme::DEMClumpBatch> (deme::DEMSolver::*)(
                 const std::shared_ptr<deme::DEMClumpBatch>&)>(&deme::DEMSolver::Duplicate),
             "Duplicate a batch of clumps that is loaded into the system.")
        .def("AddExternalObject", &deme::DEMSolver::AddExternalObject,
             "Add an analytical object to the simulation system.")
        .def(
            "SetOutputContent",
            static_cast<void (deme::DEMSolver::*)(const std::vector<std::string>&)>(&deme::DEMSolver::SetOutputContent),
            "Specify the information that needs to go into the clump or sphere output files.")
        .def("SetMeshOutputFormat",
             static_cast<void (deme::DEMSolver::*)(const std::string&)>(&deme::DEMSolver::SetMeshOutputFormat),
             "Specify the output file format of meshes.")
        .def("SetContactOutputContent",
             static_cast<void (deme::DEMSolver::*)(const std::vector<std::string>&)>(
                 &deme::DEMSolver::SetContactOutputContent),
             "Specify the information that needs to go into the contact pair output files.")
        .def("SetContactOutputFormat",
             static_cast<void (deme::DEMSolver::*)(const std::string&)>(&deme::DEMSolver::SetContactOutputFormat),
             "Specify the file format of contact pairs.")
        .def("SetVerbosity", static_cast<void (deme::DEMSolver::*)(const std::string&)>(&deme::DEMSolver::SetVerbosity),
             "Set the verbosity level of the solver.")
        .def("LoadMaterial",
             static_cast<std::shared_ptr<deme::DEMMaterial> (deme::DEMSolver::*)(
                 const std::unordered_map<std::string, float>&)>(&deme::DEMSolver::LoadMaterial),
             "Load materials properties (Young's modulus, Poisson's ratio...) into the system.")
        .def("LoadMaterial",
             static_cast<std::shared_ptr<deme::DEMMaterial> (deme::DEMSolver::*)(deme::DEMMaterial&)>(
                 &deme::DEMSolver::LoadMaterial),
             "Load materials properties into the system.")
        .def("InstructBoxDomainDimension",
             static_cast<void (deme::DEMSolver::*)(float, float, float, const std::string&)>(
                 &deme::DEMSolver::InstructBoxDomainDimension),
             "Set the Box Domain Dimension", py::arg("x"), py::arg("y"), py::arg("z"), py::arg("dir_exact") = "none")
        .def("InstructBoxDomainDimension",
             static_cast<void (deme::DEMSolver::*)(const std::pair<float, float>&, const std::pair<float, float>&,
                                                   const std::pair<float, float>&, const std::string& dir_exact)>(
                 &deme::DEMSolver::InstructBoxDomainDimension),
             "Set the span of the Box Domain", py::arg("x"), py::arg("y"), py::arg("z"), py::arg("dir_exact") = "none")
        .def("InstructBoxDomainBoundingBC", &deme::DEMSolver::InstructBoxDomainBoundingBC,
             "Instruct if and how we should add boundaries to the simulation world upon initialization. Choose between "
             "`none', `all' (add 6 boundary planes) and `top_open' (add 5 boundary planes and leave the z-directon top "
             "open). Also specifies the material that should be assigned to those bounding boundaries.")
        .def("SetMaterialPropertyPair", &deme::DEMSolver::SetMaterialPropertyPair,
             "Set the value for a material property that by nature involves a pair of a materials (e.g. friction "
             "coefficient).")
        .def("AddBCPlane",
             static_cast<std::shared_ptr<deme::DEMExternObj> (deme::DEMSolver::*)(
                 const std::vector<float>&, const std::vector<float>&, const std::shared_ptr<deme::DEMMaterial>&)>(
                 &deme::DEMSolver::AddBCPlane),
             "Add an analytical plane to the simulation.")
        .def("Track", (&deme::DEMSolver::PythonTrack),
             "Create a DEMTracker to allow direct control/modification/query to this external object/batch of "
             "clumps/triangle mesh object.")
        .def("AddWavefrontMeshObject",
             static_cast<std::shared_ptr<deme::DEMMeshConnected> (deme::DEMSolver::*)(
                 const std::string&, const std::shared_ptr<deme::DEMMaterial>&, bool, bool)>(
                 &deme::DEMSolver::AddWavefrontMeshObject),
             "Load a mesh-represented object.", py::arg("filename"), py::arg("mat"), py::arg("load_normals") = true,
             py::arg("load_uv") = false)
        .def("AddWavefrontMeshObject",
             static_cast<std::shared_ptr<deme::DEMMeshConnected> (deme::DEMSolver::*)(deme::DEMMeshConnected&)>(
                 &deme::DEMSolver::AddWavefrontMeshObject),
             "Load a mesh-represented object.")
        .def("AddWavefrontMeshObject",
             static_cast<std::shared_ptr<deme::DEMMeshConnected> (deme::DEMSolver::*)(
                 const std::string& filename, bool, bool)>(&deme::DEMSolver::AddWavefrontMeshObject),
             "Load a mesh-represented object.", py::arg("filename"), py::arg("load_normals") = true,
             py::arg("load_uv") = false)
        .def("LoadClumpType",
             static_cast<std::shared_ptr<deme::DEMClumpTemplate> (deme::DEMSolver::*)(
                 float, const std::vector<float>&, const std::string, const std::shared_ptr<deme::DEMMaterial>&)>(
                 &deme::DEMSolver::LoadClumpType),
             "Load a clump type into the API-level cache")
        .def("LoadClumpType",
             static_cast<std::shared_ptr<deme::DEMClumpTemplate> (deme::DEMSolver::*)(
                 float mass, const std::vector<float>&, const std::vector<float>&,
                 const std::vector<std::vector<float>>&, const std::shared_ptr<deme::DEMMaterial>&)>(
                 &deme::DEMSolver::LoadClumpType),
             "Load a clump type into the API-level cache")
        .def("LoadClumpType",
             static_cast<std::shared_ptr<deme::DEMClumpTemplate> (deme::DEMSolver::*)(deme::DEMClumpTemplate&)>(
                 &deme::DEMSolver::LoadClumpType),
             "Load a clump type into the API-level cache")
        .def("LoadClumpType",
             static_cast<std::shared_ptr<deme::DEMClumpTemplate> (deme::DEMSolver::*)(
                 float, const std::vector<float>&, const std::string,
                 const std::vector<std::shared_ptr<deme::DEMMaterial>>&)>(&deme::DEMSolver::LoadClumpType),
             "Load a clump type into the API-level cache")
        .def("LoadClumpType",
             static_cast<std::shared_ptr<deme::DEMClumpTemplate> (deme::DEMSolver::*)(
                 float, const std::vector<float>&, const std::string, const std::shared_ptr<deme::DEMMaterial>&)>(
                 &deme::DEMSolver::LoadClumpType),
             "Load a clump type into the API-level cache")

        .def("GetClumpContacts",
             static_cast<std::vector<std::pair<deme::bodyID_t, deme::bodyID_t>> (deme::DEMSolver::*)() const>(
                 &deme::DEMSolver::GetClumpContacts),
             "Get all clump--clump contacts in the simulation system.")

        .def("GetClumpContacts",
             static_cast<std::vector<std::pair<deme::bodyID_t, deme::bodyID_t>> (deme::DEMSolver::*)(
                 const std::set<deme::family_t>&) const>(&deme::DEMSolver::GetClumpContacts),
             "Get all clump--clump contacts in the simulation system.")

        .def("GetClumpContacts",
             static_cast<std::vector<std::pair<deme::bodyID_t, deme::bodyID_t>> (deme::DEMSolver::*)(
                 std::vector<std::pair<deme::family_t, deme::family_t>>&) const>(&deme::DEMSolver::GetClumpContacts),
             "Get all clump--clump contacts in the simulation system.")

        .def("AddClumps",
             static_cast<std::shared_ptr<deme::DEMClumpBatch> (deme::DEMSolver::*)(deme::DEMClumpBatch&)>(
                 &deme::DEMSolver::AddClumps),
             "Load input clumps (topology types and initial locations) on a per-pair basis. Note that the initial "
             "location means the location of the clumps' CoM coordinates in the global frame.")
        .def("AddClumps",
             static_cast<std::shared_ptr<deme::DEMClumpBatch> (deme::DEMSolver::*)(
                 std::shared_ptr<deme::DEMClumpTemplate>&, const std::vector<std::vector<float>>&)>(
                 &deme::DEMSolver::AddClumps),
             "Load input clumps (topology types and initial locations) on a per-pair basis. Note that the initial "
             "location means the location of the clumps' CoM coordinates in the global frame.")
        .def("AddClumps",
             static_cast<std::shared_ptr<deme::DEMClumpBatch> (deme::DEMSolver::*)(
                 const std::vector<std::shared_ptr<deme::DEMClumpTemplate>>&, const std::vector<std::vector<float>>&)>(
                 &deme::DEMSolver::AddClumps),
             "Load input clumps (topology types and initial locations) on a per-pair basis. Note that the initial "
             "location means the location of the clumps' CoM coordinates in the global frame.")

        .def("SetFamilyFixed", &deme::DEMSolver::SetFamilyFixed, "Mark all entities in this family to be fixed.")

        .def("SetFamilyPrescribedAngVel",
             static_cast<void (deme::DEMSolver::*)(unsigned int, const std::string&, const std::string&,
                                                   const std::string&, bool)>(
                 &deme::DEMSolver::SetFamilyPrescribedAngVel),
             "Let the linear velocities of all entites in this family always keep `as is', and not influenced by the "
             "force exerted from other simulation entites.",
             py::arg("ID"), py::arg("velX"), py::arg("velY"), py::arg("velZ"), py::arg("dictate") = true)
        .def("SetFamilyPrescribedAngVel",
             static_cast<void (deme::DEMSolver::*)(unsigned int)>(&deme::DEMSolver::SetFamilyPrescribedAngVel),
             "Let the linear velocities of all entites in this family always keep `as is', and not influenced by the "
             "force exerted from other simulation entites.")

        .def("SetFamilyPrescribedLinVel",
             static_cast<void (deme::DEMSolver::*)(unsigned int, const std::string&, const std::string&,
                                                   const std::string&, bool)>(
                 &deme::DEMSolver::SetFamilyPrescribedLinVel),
             "Set the prescribed linear velocity to all entities in a family. If dictate is set to true, then this "
             "family will not be influenced by the force exerted from other simulation entites (both linear and "
             "rotational motions).",
             py::arg("ID"), py::arg("velX"), py::arg("velY"), py::arg("velZ"), py::arg("dictate") = true)
        .def("SetFamilyPrescribedLinVel",
             static_cast<void (deme::DEMSolver::*)(unsigned int)>(&deme::DEMSolver::SetFamilyPrescribedLinVel),
             "Let the linear velocities of all entites in this family always keep `as is', and not influenced by the "
             "force exerted from other simulation entites.")

        .def("SetFamilyPrescribedPosition",
             static_cast<void (deme::DEMSolver::*)(unsigned int, const std::string&, const std::string&,
                                                   const std::string&, bool)>(
                 &deme::DEMSolver::SetFamilyPrescribedPosition),
             "Keep the positions of all entites in this family to remain exactly the user-specified values.",
             py::arg("ID"), py::arg("velX"), py::arg("velY"), py::arg("velZ"), py::arg("dictate") = true)
        .def("SetFamilyPrescribedPosition",
             static_cast<void (deme::DEMSolver::*)(unsigned int)>(&deme::DEMSolver::SetFamilyPrescribedPosition),
             "Keep the positions of all entites in this family to remain as is.")

        .def("SetFamilyPrescribedQuaternion",
             static_cast<void (deme::DEMSolver::*)(unsigned int, const std::string&, bool)>(
                 &deme::DEMSolver::SetFamilyPrescribedQuaternion),
             "Keep the orientation quaternions of all entites in this family to remain exactly the user-specified "
             "values.",
             py::arg("ID"), py::arg("q_formula"), py::arg("dictate") = true)
        .def("SetFamilyPrescribedQuaternion",
             static_cast<void (deme::DEMSolver::*)(unsigned int)>(&deme::DEMSolver::SetFamilyPrescribedQuaternion),
             "Let the orientation quaternions of all entites in this family always keep `as is'.")

        .def("AddFamilyPrescribedAcc", &deme::DEMSolver::AddFamilyPrescribedAcc,
             "The entities in this family will always experienced an extra acceleration defined using this method.")
        .def("AddFamilyPrescribedAngAcc", &deme::DEMSolver::AddFamilyPrescribedAngAcc,
             "The entities in this family will always experienced an extra angular acceleration defined using this "
             "method.")

        .def("SetContactWildcards", &deme::DEMSolver::SetContactWildcards,
             "Set the names for the extra quantities that will be associated with each contact pair.")
        .def("SetOwnerWildcards", &deme::DEMSolver::SetOwnerWildcards,
             "Set the names for the extra quantities that will be associated with each owner.")
        .def("SetGeometryWildcards", &deme::DEMSolver::SetGeometryWildcards,
             "Set the names for the extra quantities that will be associated with each geometry entity (such as "
             "sphere, triangle).")

        .def("SetFamilyContactWildcardValueAny", &deme::DEMSolver::SetFamilyContactWildcardValueAny,
             "Change the value of contact wildcards to val if either of the contact geometries is in family N.")
        .def("SetFamilyContactWildcardValueAll", &deme::DEMSolver::SetFamilyContactWildcardValueAll,
             "Change the value of contact wildcards to val if both of the contact geometries are in family N.")
        .def("SetFamilyContactWildcardValue", &deme::DEMSolver::SetFamilyContactWildcardValue,
             "Change the value of contact wildcards to val if one of the contact geometry is in family N1, and the "
             "other is in N2.")
        .def("SetContactWildcardValue", &deme::DEMSolver::SetContactWildcardValue,
             "Change the value of contact wildcards to val. Apply to all simulation bodies that are present.")

        .def("SetFamilyOwnerWildcardValue",
             static_cast<void (deme::DEMSolver::*)(unsigned int N, const std::string& name,
                                                   const std::vector<float>& vals)>(
                 &deme::DEMSolver::SetFamilyOwnerWildcardValue),
             "Modify the owner wildcard's values of all entities in family N.")
        .def("SetFamilyOwnerWildcardValue",
             static_cast<void (deme::DEMSolver::*)(unsigned int N, const std::string& name, float val)>(
                 &deme::DEMSolver::SetFamilyOwnerWildcardValue),
             "Modify the owner wildcard's values of all entities in family N.")

        .def("SetFamilyClumpMaterial", &deme::DEMSolver::SetFamilyClumpMaterial,
             "Set all clumps in this family to have this material.")
        .def("SetFamilyMeshMaterial", &deme::DEMSolver::SetFamilyMeshMaterial,
             "Set all meshes in this family to have this material.")
        .def("SetFamilyExtraMargin", &deme::DEMSolver::SetFamilyExtraMargin,
             "Add an extra contact margin to entities in a family so they are registered as potential contact pairs "
             "earlier.")

        .def("GetAllOwnerWildcardValue", &deme::DEMSolver::GetAllOwnerWildcardValue,
             "Get the owner wildcard's values of all entities.")
        .def("GetFamilyOwnerWildcardValue", &deme::DEMSolver::GetFamilyOwnerWildcardValue,
             "Get the owner wildcard's values of all entities in family N.")

        .def("ClearCache", &deme::DEMSolver::ClearCache,
             "Remove host-side cached vectors (so you can re-define them, and then re-initialize system).")

        .def("CreateInspector",
             static_cast<std::shared_ptr<deme::DEMInspector> (deme::DEMSolver::*)(const std::string&)>(
                 &deme::DEMSolver::CreateInspector),
             "Create a inspector object that can help query some statistical info of the clumps in the simulation.",
             py::arg("quantity") = "clump_max_z")
        .def("GetNumClumps", &deme::DEMSolver::GetNumClumps)
        .def("CreateInspector",
             static_cast<std::shared_ptr<deme::DEMInspector> (deme::DEMSolver::*)(
                 const std::string&, const std::string&)>(&deme::DEMSolver::CreateInspector),
             "Create a inspector object that can help query some statistical info of the clumps in the simulation.")
        .def("SetInitTimeStep", &deme::DEMSolver::SetInitTimeStep,
             "Set the initial time step size. If using constant step size, then this will be used throughout; "
             "otherwise, the actual step size depends on the variable step strategy.")
        .def("SetGravitationalAcceleration",
             static_cast<void (deme::DEMSolver::*)(const std::vector<float>&)>(
                 &deme::DEMSolver::SetGravitationalAcceleration),
             "Set gravitational pull")
        .def("SetMaxVelocity", &deme::DEMSolver::SetMaxVelocity,
             "Set the maximum expected particle velocity. The solver will not use a velocity larger than this for "
             "determining the margin thickness, and velocity larger than this will be considered a system anomaly.")
        .def("SetErrorOutVelocity", &deme::DEMSolver::SetErrorOutVelocity,
             "Set the velocity which when exceeded, the solver errors out. A huge number can be used to discourage "
             "this error type. Defaulted to 5e4.")
        .def("SetExpandSafetyMultiplier", &deme::DEMSolver::SetExpandSafetyMultiplier,
             "Assign a multiplier to our estimated maximum system velocity, when deriving the thinckness of the "
             "contact `safety' margin.")
        .def("Initialize", &deme::DEMSolver::Initialize, "Initializes the system.")
        .def("WriteSphereFile", &deme::DEMSolver::WriteSphereFile, "Writes a sphere file.")
        .def("WriteMeshFile", &deme::DEMSolver::WriteMeshFile, "Write the current status of all meshes to a file.")
        .def("WriteClumpFile", &deme::DEMSolver::WriteClumpFile, "Write the current status of clumps to a file.",
             py::arg("outfilename"), py::arg("accuracy") = 10)
        .def("WriteContactFile", &deme::DEMSolver::WriteContactFile,
             "Write all contact pairs to a file. Forces smaller than threshold will not be outputted.",
             py::arg("outfilename"), py::arg("force_thres") = 1e-15)
        // Maybe add checkpoint-reading methods here...
        .def("DoDynamics", &deme::DEMSolver::DoDynamics,
             "Advance simulation by this amount of time (but does not attempt to sync kT and dT). This can work with "
             "both long and short call durations and allows interplay with co-simulation APIs.")
        .def("DoStepDynamics", &deme::DEMSolver::DoStepDynamics,
             "Equivalent to calling DoDynamics with the time step size as the argument.")
        .def("DoDynamicsThenSync", &deme::DEMSolver::DoDynamicsThenSync,
             "Advance simulation by this amount of time, and at the end of this call, synchronize kT and dT. This is "
             "suitable for a longer call duration and without co-simulation.")
        .def("UpdateSimParams", &deme::DEMSolver::UpdateSimParams,
             "Transfer the cached sim params to the workers. Used for sim environment modification after system "
             "initialization.")
        .def("ChangeFamily", &deme::DEMSolver::ChangeFamily,
             "Change all entities with family number ID_from to have a new number ID_to, when the condition defined by "
             "the string is satisfied by the entities in question. This should be called before initialization, and "
             "will be baked into the solver, so the conditions will be checked and changes applied every time step.")
        .def("ChangeFamilyWhen", &deme::DEMSolver::ChangeFamilyWhen,
             "Change all entities with family number ID_from to have a new number ID_to, when the condition defined by "
             "the string is satisfied by the entities in question. This should be called before initialization, and "
             "will be baked into the solver, so the conditions will be checked and changes applied every time step.")
        .def("ChangeClumpFamily", &deme::DEMSolver::ChangeClumpFamily,
             "Change the family number for the clumps in a box region to the specified value.", py::arg("fam_num"),
             py::arg("X") = std::pair<double, double>(-DEME_HUGE_FLOAT, DEME_HUGE_FLOAT),
             py::arg("Y") = std::pair<double, double>(-DEME_HUGE_FLOAT, DEME_HUGE_FLOAT),
             py::arg("Z") = std::pair<double, double>(-DEME_HUGE_FLOAT, DEME_HUGE_FLOAT),
             py::arg("orig_fam") = std::set<unsigned int>())

        .def("ShowThreadCollaborationStats", &deme::DEMSolver::ShowThreadCollaborationStats,
             "Show the collaboration stats between dT and kT. This is more useful for tweaking the number of time "
             "steps that dT should be allowed to be in advance of kT.")
        .def("ShowTimingStats", &deme::DEMSolver::ShowTimingStats,
             "Show the wall time and percentages of wall time spend on various solver tasks.")
        .def("ShowAnomalies", &deme::DEMSolver::ShowAnomalies,
             "Show potential anomalies that may have been there in the simulation, then clear the anomaly log.")
        .def("ClearThreadCollaborationStats", &deme::DEMSolver::ClearThreadCollaborationStats,
             "Reset the collaboration stats between dT and kT back to the initial value (0). You should call this if "
             "you want to start over and re-inspect the stats of the new run; otherwise, it is generally not needed, "
             "you can go ahead and destroy DEMSolver.")
        .def("ClearTimingStats", &deme::DEMSolver::ClearTimingStats,
             "Reset the recordings of the wall time and percentages of wall time spend on various solver tasks.")
        .def("PurgeFamily", &deme::DEMSolver::PurgeFamily)
        .def("ReleaseFlattenedArrays", &deme::DEMSolver::ReleaseFlattenedArrays)
        .def("GetWhetherForceCollectInKernel", &deme::DEMSolver::GetWhetherForceCollectInKernel,
             "Return whether the solver is currently reducing force in the force calculation kernel.")
        .def("AddOwnerNextStepAcc", &deme::DEMSolver::AddOwnerNextStepAcc,
             "Add an extra acceleration to a owner for the next time step.")
        .def("AddOwnerNextStepAngAcc", &deme::DEMSolver::AddOwnerNextStepAngAcc,
             " Add an extra angular acceleration to a owner for the next time step.")
        .def("DisableContactBetweenFamilies", &deme::DEMSolver::DisableContactBetweenFamilies,
             "Instruct the solver that the 2 input families should not have contacts (a.k.a. ignored, if such a pair "
             "is encountered in contact detection). These 2 families can be the same (which means no contact within "
             "members of that family).")
        .def("EnableContactBetweenFamilies", &deme::DEMSolver::EnableContactBetweenFamilies,
             "Re-enable contact between 2 families after the system is initialized.")
        .def("DisableFamilyOutput", &deme::DEMSolver::DisableFamilyOutput,
             "Prevent entites associated with this family to be outputted to files.");

    py::class_<deme::DEMMaterial, std::shared_ptr<deme::DEMMaterial>>(obj, "DEMMaterial")
        .def(py::init<const std::unordered_map<std::string, float>&>())
        .def_readwrite("mat_prop", &deme::DEMMaterial::mat_prop)
        .def_readwrite("load_order", &deme::DEMMaterial::load_order);

    py::class_<deme::DEMClumpTemplate, std::shared_ptr<deme::DEMClumpTemplate>>(obj, "DEMClumpTemplate")
        .def(py::init<>())
        .def("Mass", &deme::DEMClumpTemplate::GetMass)
        .def("MOI", &deme::DEMClumpTemplate::GetMOI)
        .def("SetMass", &deme::DEMClumpTemplate::SetMass)
        .def("SetMOI",
             static_cast<void (deme::DEMClumpTemplate::*)(const std::vector<float>&)>(&deme::DEMClumpTemplate::SetMOI))
        .def("SetMaterial",
             static_cast<void (deme::DEMClumpTemplate::*)(const std::vector<std::shared_ptr<deme::DEMMaterial>>&)>(
                 &deme::DEMClumpTemplate::SetMaterial))
        .def("SetMaterial",
             static_cast<void (deme::DEMClumpTemplate::*)(const std::shared_ptr<deme::DEMMaterial>& input)>(
                 &deme::DEMClumpTemplate::SetMaterial))
        .def("SetVolume", &deme::DEMClumpTemplate::SetVolume)
        .def("ReadComponentFromFile", &deme::DEMClumpTemplate::ReadComponentFromFile,
             "Retrieve clump's sphere component information from a file", py::arg("filename"), py::arg("x_id") = "x",
             py::arg("y_id") = "y", py::arg("z_id") = "z", py::arg("r_id") = "r")
        .def("InformCentroidPrincipal",
             static_cast<void (deme::DEMClumpTemplate::*)(const std::vector<float>&, const std::vector<float>&)>(
                 &deme::DEMClumpTemplate::InformCentroidPrincipal))
        .def("Move",
             static_cast<void (deme::DEMClumpTemplate::*)(const std::vector<float>&, const std::vector<float>&)>(
                 &deme::DEMClumpTemplate::Move))
        .def("Scale", &deme::DEMClumpTemplate::Scale)
        .def("AssignName", &deme::DEMClumpTemplate::AssignName);

    py::class_<deme::DEMClumpBatch, deme::DEMInitializer, std::shared_ptr<deme::DEMClumpBatch>>(obj, "DEMClumpBatch")
        .def(py::init<size_t&>())
        .def("GetNumClumps", &deme::DEMClumpBatch::GetNumClumps)
        .def("GetNumSpheres", &deme::DEMClumpBatch::GetNumSpheres)
        .def("SetTypes",
             static_cast<void (deme::DEMClumpBatch::*)(const std::vector<std::shared_ptr<deme::DEMClumpTemplate>>&)>(
                 &deme::DEMClumpBatch::SetTypes))
        .def("SetTypes", static_cast<void (deme::DEMClumpBatch::*)(const std::shared_ptr<deme::DEMClumpTemplate>&)>(
                             &deme::DEMClumpBatch::SetTypes))
        .def("SetType", &deme::DEMClumpBatch::SetType)
        .def("SetVel", static_cast<void (deme::DEMClumpBatch::*)(const std::vector<std::vector<float>>&)>(
                           &deme::DEMClumpBatch::SetVel))
        .def("SetVel",
             static_cast<void (deme::DEMClumpBatch::*)(const std::vector<float>&)>(&deme::DEMClumpBatch::SetVel))
        //    .def("SetAngVel",
        //         static_cast<void (deme::DEMClumpBatch::*)(const
        //         std::vector<float3>&)>(&deme::DEMClumpBatch::SetAngVel))
        .def("SetFamilies", static_cast<void (deme::DEMClumpBatch::*)(const std::vector<unsigned int>&)>(
                                &deme::DEMClumpBatch::SetFamilies))
        .def("SetFamilies", static_cast<void (deme::DEMClumpBatch::*)(unsigned int)>(&deme::DEMClumpBatch::SetFamilies))
        .def("SetFamily", &deme::DEMClumpBatch::SetFamily)
        .def("SetExistingContacts", &deme::DEMClumpBatch::SetExistingContacts)
        .def("SetExistingContactWildcards", &deme::DEMClumpBatch::SetExistingContactWildcards)
        .def("AddExistingContactWildcard", &deme::DEMClumpBatch::AddExistingContactWildcard)
        .def("SetOwnerWildcards", &deme::DEMClumpBatch::SetOwnerWildcards)
        .def("AddOwnerWildcard",
             static_cast<void (deme::DEMClumpBatch::*)(const std::string&, const std::vector<float>&)>(
                 &deme::DEMClumpBatch::AddOwnerWildcard))
        .def("AddOwnerWildcard", static_cast<void (deme::DEMClumpBatch::*)(const std::string&, float)>(
                                     &deme::DEMClumpBatch::AddOwnerWildcard))
        .def("SetGeometryWildcards", &deme::DEMClumpBatch::SetGeometryWildcards)
        .def("AddGeometryWildcard",
             static_cast<void (deme::DEMClumpBatch::*)(const std::string&, const std::vector<float>&)>(
                 &deme::DEMClumpBatch::AddGeometryWildcard))
        .def("AddGeometryWildcard", static_cast<void (deme::DEMClumpBatch::*)(const std::string&, float)>(
                                        &deme::DEMClumpBatch::AddGeometryWildcard))
        .def("GetNumContacts", &deme::DEMClumpBatch::GetNumContacts);

    py::class_<deme::DEMExternObj, deme::DEMInitializer, std::shared_ptr<deme::DEMExternObj>>(obj, "DEMExternObj")
        .def(py::init<>())
        .def("Mass", &deme::DEMExternObj::GetMass)
        .def("MOI", &deme::DEMExternObj::GetMOI)
        .def("SetFamily", &deme::DEMExternObj::SetFamily, "Defines an object contact family number")
        .def("SetMass", &deme::DEMExternObj::SetMass, "Sets the mass of this object")
        .def("SetMOI",
             static_cast<void (deme::DEMExternObj::*)(const std::vector<float>&)>(&deme::DEMExternObj::SetMOI),
             "Sets the MOI (in the principal frame)")
        .def("SetInitQuat",
             static_cast<void (deme::DEMExternObj::*)(const std::vector<float>&)>(&deme::DEMExternObj::SetInitQuat),
             "Set the initial quaternion for this object (before simulation initializes).")
        .def("SetInitPos",
             static_cast<void (deme::DEMExternObj::*)(const std::vector<float>&)>(&deme::DEMExternObj::SetInitPos),
             "Set the initial position for this object (before simulation initializes).")
        .def("AddPlane",
             static_cast<void (deme::DEMExternObj::*)(const std::vector<float>&, const std::vector<float>&,
                                                      const std::shared_ptr<deme::DEMMaterial>&)>(
                 &deme::DEMExternObj::AddPlane),
             "Add a plane with infinite size.")
        //.def("AddPlate", static_cast<void (&deme::DEMExternObj::AddPlate, "Add a plate with finite size.")
        .def("AddZCylinder",
             static_cast<void (deme::DEMExternObj::*)(const std::vector<float>&, const float,
                                                      const std::shared_ptr<deme::DEMMaterial>&,
                                                      const deme::objNormal_t)>(&deme::DEMExternObj::AddZCylinder),
             "Add a z-axis-aligned cylinder of infinite length", py::arg("pos"), py::arg("rad"), py::arg("material"),
             py::arg("normal") = deme::ENTITY_NORMAL_INWARD)
        .def("AddCylinder",
             static_cast<void (deme::DEMExternObj::*)(const std::vector<float>&, const std::vector<float>&, const float,
                                                      const std::shared_ptr<deme::DEMMaterial>&,
                                                      const deme::objNormal_t)>(&deme::DEMExternObj::AddCylinder),
             "Add a cylinder of infinite length, which is along a user-specific axis", py::arg("pos"), py::arg("axis"),
             py::arg("rad"), py::arg("material"), py::arg("normal") = deme::ENTITY_NORMAL_INWARD)
        .def_readwrite("types", &deme::DEMExternObj::types)
        .def_readwrite("materials", &deme::DEMExternObj::materials)
        .def_readwrite("family_code", &deme::DEMExternObj::family_code)
        .def_readwrite("init_pos", &deme::DEMExternObj::init_pos)
        .def_readwrite("init_oriQ", &deme::DEMExternObj::init_oriQ)
        .def_readwrite("mass", &deme::DEMExternObj::mass)
        .def_readwrite("MOI", &deme::DEMExternObj::MOI)
        .def_readwrite("load_order", &deme::DEMExternObj::load_order)
        .def_readwrite("entity_params", &deme::DEMExternObj::entity_params);

    py::class_<deme::DEMMeshConnected, deme::DEMInitializer, std::shared_ptr<deme::DEMMeshConnected>>(
        obj, "DEMMeshConnected")
        .def(py::init<>())
        .def(py::init<std::string&>())
        .def(py::init<std::string, const std::shared_ptr<deme::DEMMaterial>&>())
        .def("Mass", &deme::DEMMeshConnected::GetMass)
        .def("MOI", &deme::DEMMeshConnected::GetMOI)
        .def("Clear", &deme::DEMMeshConnected::Clear, "Clears everything from memory")
        .def("LoadWavefrontMesh", &deme::DEMMeshConnected::LoadWavefrontMesh,
             "Load a triangle mesh saved as a Wavefront .obj file", py::arg("input_file"),
             py::arg("load_normals") = true, py::arg("load_uv") = false)
        .def("WriteWavefront", &deme::DEMMeshConnected::WriteWavefront,
             "Write the specified meshes in a Wavefront .obj file")
        .def("GetNumTriangles", &deme::DEMMeshConnected::GetNumTriangles,
             "Get the number of triangles already added to this mesh")
        .def("GetNumNodes", &deme::DEMMeshConnected::GetNumNodes, "Get the number of nodes in the mesh")
        .def("UseNormals", &deme::DEMMeshConnected::UseNormals,
             "Instruct that when the mesh is initialized into the system, it will re-order the nodes of each triangle "
             "so that the normals derived from right-hand-rule are the same as the normals in the mesh file",
             py::arg("use") = true)
        .def("GetTriangle", &deme::DEMMeshConnected::GetTriangle, "Access the n-th triangle in mesh")
        .def("SetMass", &deme::DEMMeshConnected::SetMass)
        .def("SetMOI",
             static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&)>(&deme::DEMMeshConnected::SetMOI))
        .def("SetFamily", &deme::DEMMeshConnected::SetFamily)
        .def("SetMaterial", static_cast<void (deme::DEMMeshConnected::*)(const std::shared_ptr<deme::DEMMaterial>&)>(
                                &deme::DEMMeshConnected::SetMaterial))
        .def("SetMaterial",
             static_cast<void (deme::DEMMeshConnected::*)(const std::vector<std::shared_ptr<deme::DEMMaterial>>&)>(
                 &deme::DEMMeshConnected::SetMaterial))
        .def("SetInitQuat", static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&)>(
                                &deme::DEMMeshConnected::SetInitQuat))
        .def("SetInitPos", static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&)>(
                               &deme::DEMMeshConnected::SetInitPos))
        .def("InformCentroidPrincipal",
             static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&, const std::vector<float>&)>(
                 &deme::DEMMeshConnected::InformCentroidPrincipal))
        .def("Move",
             static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&, const std::vector<float>&)>(
                 &deme::DEMMeshConnected::Move))
        .def("Mirror",
             static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&, const std::vector<float>&)>(
                 &deme::DEMMeshConnected::Mirror))
        .def("Scale", static_cast<void (deme::DEMMeshConnected::*)(float)>(&deme::DEMMeshConnected::Scale))
        .def("Scale",
             static_cast<void (deme::DEMMeshConnected::*)(const std::vector<float>&)>(&deme::DEMMeshConnected::Scale))
        .def("ClearWildcards", &deme::DEMMeshConnected::ClearWildcards)
        .def("SetGeometryWildcards", &deme::DEMMeshConnected::SetGeometryWildcards)
        .def("AddGeometryWildcard",
             static_cast<void (deme::DEMMeshConnected::*)(const std::string&, const std::vector<float>&)>(
                 &deme::DEMMeshConnected::AddGeometryWildcard))
        .def("AddGeometryWildcard", static_cast<void (deme::DEMMeshConnected::*)(const std::string&, float)>(
                                        &deme::DEMMeshConnected::AddGeometryWildcard))
        .def("GetCoordsVertices", &deme::DEMMeshConnected::GetCoordsVerticesAsVectorOfVectors)
        //.def("GetCoordsUV", &deme::DEMMeshConnected::GetCoordsUVPython)
        //.def("GetCoordsColors", &deme::DEMMeshConnected::GetCoordsColorsPython)
        .def("GetIndicesVertexes", &deme::DEMMeshConnected::GetIndicesVertexesAsVectorOfVectors);
    //    .def("GetIndicesNormals", &deme::DEMMeshConnected::GetIndicesNormalsPython)
    //    .def("GetIndicesUV", &deme::DEMMeshConnected::GetIndicesUVPython)
    //    .def("GetIndicesColors", &deme::DEMMeshConnected::GetIndicesColorsPython);

    //// TODO: Insert readwrite functions to access all public class objects!

    py::enum_<deme::VERBOSITY>(obj, "VERBOSITY")
        .value("QUIET", deme::VERBOSITY::QUIET)
        .value("ERROR", deme::VERBOSITY::DEME_ERROR)
        .value("WARNING", deme::VERBOSITY::WARNING)
        .value("INFO", deme::VERBOSITY::INFO)
        .value("STEP_ANOMALY", deme::VERBOSITY::STEP_ANOMALY)
        .value("STEP_METRIC", deme::VERBOSITY::STEP_METRIC)
        .value("DEBUG", deme::VERBOSITY::DEBUG)
        .value("STEP_DEBUG", deme::VERBOSITY::STEP_DEBUG)
        .export_values();

    py::enum_<deme::TIME_INTEGRATOR>(obj, "TIME_INTEGRATOR")
        .value("FORWARD_EULER", deme::TIME_INTEGRATOR::FORWARD_EULER)
        .value("CENTERED_DIFFERENCE", deme::TIME_INTEGRATOR::CENTERED_DIFFERENCE)
        .value("EXTENDED_TAYLOR", deme::TIME_INTEGRATOR::EXTENDED_TAYLOR)
        .value("CHUNG", deme::TIME_INTEGRATOR::CHUNG)
        .export_values();

    py::enum_<deme::OWNER_TYPE>(obj, "OWNER_TYPE")
        .value("CLUMP", deme::OWNER_TYPE::CLUMP)
        .value("ANALYTICAL", deme::OWNER_TYPE::ANALYTICAL)
        .value("MESH", deme::OWNER_TYPE::MESH)
        .export_values();

    py::enum_<deme::FORCE_MODEL>(obj, "FORCE_MODEL")
        .value("HERTZIAN", deme::FORCE_MODEL::HERTZIAN)
        .value("HERTZIAN_FRICTIONLES", deme::FORCE_MODEL::HERTZIAN_FRICTIONLESS)
        .value("CUSTOM", deme::FORCE_MODEL::CUSTOM)
        .export_values();

    py::enum_<deme::OUTPUT_CONTENT>(obj, "OUTPUT_CONTENT")
        .value("XYZ", deme::OUTPUT_CONTENT::XYZ)
        .value("QUAT", deme::OUTPUT_CONTENT::QUAT)
        .value("ABSV", deme::OUTPUT_CONTENT::ABSV)
        .value("VEL", deme::OUTPUT_CONTENT::VEL)
        .value("ANG_VEL", deme::OUTPUT_CONTENT::ANG_VEL)
        .value("ABS_ACC", deme::OUTPUT_CONTENT::ABS_ACC)
        .value("ACC", deme::OUTPUT_CONTENT::ACC)
        .value("ANG_ACC", deme::OUTPUT_CONTENT::ANG_ACC)
        .value("FAMILY", deme::OUTPUT_CONTENT::FAMILY)
        .value("MAT", deme::OUTPUT_CONTENT::MAT)
        .value("OWNER_WILDCARD", deme::OUTPUT_CONTENT::OWNER_WILDCARD)
        .value("GEO_WILDCARD", deme::OUTPUT_CONTENT::GEO_WILDCARD)
        .value("EXP_FACTOR", deme::OUTPUT_CONTENT::EXP_FACTOR)
        .export_values();

    py::enum_<deme::OUTPUT_FORMAT>(obj, "OUTPUT_FORMAT")
        .value("CSV", deme::OUTPUT_FORMAT::CSV)
        .value("BINARY", deme::OUTPUT_FORMAT::BINARY)
        .value("CHPF", deme::OUTPUT_FORMAT::CHPF)
        .export_values();

    py::enum_<deme::MESH_FORMAT>(obj, "MESH_FORMAT")
        .value("VTK", deme::MESH_FORMAT::VTK)
        .value("OBJ", deme::MESH_FORMAT::OBJ)
        .export_values();

    py::enum_<deme::SPATIAL_DIR>(obj, "SPATIAL_DIR")
        .value("X", deme::SPATIAL_DIR::X)
        .value("Y", deme::SPATIAL_DIR::Y)
        .value("Z", deme::SPATIAL_DIR::Z)
        .value("NONE", deme::SPATIAL_DIR::NONE)
        .export_values();

    py::enum_<deme::CNT_OUTPUT_CONTENT>(obj, "CNT_OUTPUT_CONTENT")
        .value("CNT_TYPE", deme::CNT_OUTPUT_CONTENT::CNT_TYPE)
        .value("FORCE", deme::CNT_OUTPUT_CONTENT::FORCE)
        .value("POINT", deme::CNT_OUTPUT_CONTENT::DEME_POINT)
        .value("COMPONENT", deme::CNT_OUTPUT_CONTENT::COMPONENT)
        .value("NORMAL", deme::CNT_OUTPUT_CONTENT::NORMAL)
        .value("TORQUE", deme::CNT_OUTPUT_CONTENT::TORQUE)
        .value("CNT_WILDCARD", deme::CNT_OUTPUT_CONTENT::CNT_WILDCARD)
        .value("OWNER", deme::CNT_OUTPUT_CONTENT::OWNER)
        .value("GEO_ID", deme::CNT_OUTPUT_CONTENT::GEO_ID)
        .value("NICKNAME", deme::CNT_OUTPUT_CONTENT::NICKNAME)
        .export_values();
}
