//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//
//	SPDX-License-Identifier: BSD-3-Clause

#include <DEM/API.h>
#include <DEM/DEMAuxClasses.h>
#include <DEM/HostSideHelpers.hpp>
#include <DEM/DEMModels.h>

namespace smug {

// =============================================================================
// DEMInspector class
// =============================================================================

const std::string DEM_INSP_CODE_SPHERE_HIGH_Z = R"V0G0N(
    quantity[sphereID] = sphereZ + myRadius;
)V0G0N";

const std::string DEM_INSP_CODE_SPHERE_LOW_Z = R"V0G0N(
    quantity[sphereID] = sphereZ - myRadius;
)V0G0N";

const std::string DEM_INSP_CODE_SPHERE_HIGH_ABSV = R"V0G0N(
    float3 relPos = make_float3(myRelPosX, myRelPosY, myRelPosZ);
    // Get owner's velocity
    float3 rotVel, linVel;
    linVel.x = granData->vX[myOwner];
    linVel.y = granData->vY[myOwner];
    linVel.z = granData->vZ[myOwner];
    // rotVel is local
    rotVel.x = granData->omgBarX[myOwner];
    rotVel.y = granData->omgBarY[myOwner];
    rotVel.z = granData->omgBarZ[myOwner];
    // 2 potential points on sphere that are the fastest
    float vel;
    {
        // It is indeed an estimation, since it accounts for center of sphere
        // only. But interestingly, a sphere's rotation about its own CoM does
        // not contribute to the size of contact detection margin, which is the
        // main reason for querying max absv for us. So, it should be fine.
        float3 pRotVel = cross(rotVel, relPos);
        // Map rotational contribution back to global
        applyOriQToVector3<float, smug::oriQ_t>(pRotVel.x, pRotVel.y, pRotVel.z, 
                                                oriQw, oriQx, oriQy, oriQz);
        vel = length(pRotVel + linVel);
    }
    quantity[sphereID] = vel;
)V0G0N";

void DEMInspector::switch_quantity_type(const std::string& quantity) {
    switch (hash_charr(quantity.c_str())) {
        case ("clump_max_z"_):
            inspection_code = DEM_INSP_CODE_SPHERE_HIGH_Z;
            reduce_flavor = DEM_CUB_REDUCE_FLAVOR::MAX;
            kernel_name = "inspectSphereProperty";
            thing_to_insp = DEM_INSPECT_ENTITY_TYPE::SPHERE;
            break;
        case ("clump_min_z"_):
            inspection_code = DEM_INSP_CODE_SPHERE_LOW_Z;
            reduce_flavor = DEM_CUB_REDUCE_FLAVOR::MIN;
            kernel_name = "inspectSphereProperty";
            thing_to_insp = DEM_INSPECT_ENTITY_TYPE::SPHERE;
            break;
        // case ("mesh_max_z"_):
        //     reduce_flavor = DEM_CUB_REDUCE_FLAVOR::MAX;
        //     break;
        // case ("clump_com_max_z"_):
        //     // inspection_code = DEM_INSP_CODE_SPHERE_HIGH_Z;
        //     reduce_flavor = DEM_CUB_REDUCE_FLAVOR::MAX;
        //     kernel_name = "inspectClumpProperty";
        //     break;
        //// TODO: void ratio will have its own query function
        // case "void_ratio"_:
        //     break;
        case ("clump_max_absv"_):
            inspection_code = DEM_INSP_CODE_SPHERE_HIGH_ABSV;
            reduce_flavor = DEM_CUB_REDUCE_FLAVOR::MAX;
            kernel_name = "inspectSphereProperty";
            thing_to_insp = DEM_INSPECT_ENTITY_TYPE::SPHERE;
            break;
        // case ("clump_absv"_):
        //     reduce_flavor = DEM_CUB_REDUCE_FLAVOR::NONE;
        //     break;
        default:
            std::stringstream ss;
            ss << quantity << " is not a known query type." << std::endl;
            throw std::runtime_error(ss.str());
    }
}

float DEMInspector::GetValue() {
    assertInit();
    return sys->dTInspectReduce(inspection_kernel, kernel_name, thing_to_insp, reduce_flavor, all_domain);
}

void DEMInspector::initializeInspector(const std::unordered_map<std::string, std::string>& Subs) {
    // Add own substitutions to it
    {
        std::unordered_map<std::string, std::string> my_subs = Subs;
        my_subs["_inRegionPolicy_"] = in_region_code;
        my_subs["_quantityQueryProcess_"] = inspection_code;
        inspection_kernel = std::make_shared<jitify::Program>(
            std::move(JitHelper::buildProgram("DEMQueryKernels", JitHelper::KERNEL_DIR / "DEMQueryKernels.cu", my_subs,
                                              {"-I" + (JitHelper::KERNEL_DIR / "..").string()})));
    }
    initialized = true;
}

// =============================================================================
// DEMTracker class
// =============================================================================

float3 DEMTracker::Pos(size_t offset) {
    return sys->GetOwnerPosition(obj->ownerID + offset);
}
float3 DEMTracker::AngVelLocal(size_t offset) {
    return sys->GetOwnerAngVel(obj->ownerID + offset);
}
float3 DEMTracker::Vel(size_t offset) {
    return sys->GetOwnerVelocity(obj->ownerID + offset);
}
float4 DEMTracker::OriQ(size_t offset) {
    return sys->GetOwnerOriQ(obj->ownerID + offset);
}
// float3 DEMTracker::Acc(size_t offset) {
//     float3 contact_acc = sys->GetOwnerAcc(obj->ownerID + offset);
//     // Contact acceleration is not total acc, we need to add gravity and manually added forces
//     //// TODO: How to do that?
// }
// float3 DEMTracker::AngAcc(size_t offset) {
//     float3 contact_angAcc = sys->GetOwnerAngAcc(obj->ownerID + offset);
//     // Contact angAcc is not total angAcc, we need to add manually added angular acc
//     //// TODO: How to do that?
// }
float3 DEMTracker::ContactAcc(size_t offset) {
    return sys->GetOwnerAcc(obj->ownerID + offset);
}
float3 DEMTracker::ContactAngAccLocal(size_t offset) {
    return sys->GetOwnerAngAcc(obj->ownerID + offset);
}

void DEMTracker::SetPos(float3 pos, size_t offset) {
    sys->SetOwnerPosition(obj->ownerID + offset, pos);
}
void DEMTracker::SetAngVel(float3 angVel, size_t offset) {
    sys->SetOwnerAngVel(obj->ownerID + offset, angVel);
}
void DEMTracker::SetVel(float3 vel, size_t offset) {
    sys->SetOwnerVelocity(obj->ownerID + offset, vel);
}
void DEMTracker::SetOriQ(float4 oriQ, size_t offset) {
    sys->SetOwnerOriQ(obj->ownerID + offset, oriQ);
}
void DEMTracker::ChangeClumpSizes(const std::vector<bodyID_t>& IDs, const std::vector<float>& factors) {
    std::vector<bodyID_t> offsetted_IDs(IDs);
    size_t offset = obj->ownerID;
    std::for_each(offsetted_IDs.begin(), offsetted_IDs.end(), [offset](bodyID_t& x) { x += offset; });
    sys->ChangeClumpSizes(offsetted_IDs, factors);
}

// =============================================================================
// DEMForceModel class
// =============================================================================

void DEMForceModel::SetForceModelType(DEM_FORCE_MODEL model_type) {
    type = model_type;
    switch (model_type) {
        case (DEM_FORCE_MODEL::HERTZIAN):
            m_must_have_mat_props = {"E", "nu", "CoR", "mu", "Crr"};
            m_force_model = DEM_HERTZIAN_FORCE_MODEL();
            // History-based model uses these history-related arrays
            m_contact_wildcards = {"delta_time", "delta_tan_x", "delta_tan_y", "delta_tan_z"};
            break;
        case (DEM_FORCE_MODEL::HERTZIAN_FRICTIONLESS):
            m_must_have_mat_props = {"E", "nu", "CoR"};
            m_force_model = DEM_HERTZIAN_FORCE_MODEL_FRICTIONLESS();
            // No contact history needed for frictionless
            m_contact_wildcards.clear();
            break;
        case (DEM_FORCE_MODEL::CUSTOM):
            m_must_have_mat_props.clear();
    }
}

void DEMForceModel::DefineCustomModel(const std::string& model) {
    // If custom model is set, we don't care what materials needs to be set
    m_must_have_mat_props.clear();
    type = DEM_FORCE_MODEL::CUSTOM;
    m_force_model = model;
}

int DEMForceModel::ReadCustomModelFile(const std::filesystem::path& sourcefile) {
    if (!std::filesystem::exists(sourcefile)) {
        return 1;
    }
    // If custom model is set, we don't care what materials needs to be set
    m_must_have_mat_props.clear();
    type = DEM_FORCE_MODEL::CUSTOM;
    m_force_model = read_file_to_string(sourcefile);
    return 0;
}

void DEMForceModel::SetPerContactWildcards(const std::set<std::string>& wildcards) {
    for (const auto& a_str : wildcards) {
        if (match_pattern(a_str, " ")) {
            // Wildcard array names cannot have spaces in them
            std::stringstream ss;
            ss << "Contact wildcard " << a_str << " is not valid: no spaces allowed in its name." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }
    m_contact_wildcards = wildcards;
}

void DEMForceModel::SetPerOwnerWildcards(const std::set<std::string>& wildcards) {
    for (const auto& a_str : wildcards) {
        if (match_pattern(a_str, " ")) {
            std::stringstream ss;
            ss << "Owner wildcard " << a_str << " is not valid: no spaces allowed in its name." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }
    m_owner_wildcards = wildcards;
}

}  // END namespace smug