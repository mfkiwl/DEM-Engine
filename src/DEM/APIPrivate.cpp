//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//  All rights reserved.

#include <core/ApiVersion.h>
#include <DEM/API.h>
#include <DEM/DEMDefines.h>
#include <DEM/HostSideHelpers.hpp>

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <limits>
#include <algorithm>

namespace sgps {

// Simple material util functions used by some DEM private methods
inline bool is_DEM_material_same(const std::shared_ptr<DEMMaterial>& a, const std::shared_ptr<DEMMaterial>& b);
inline unsigned int stash_material_in_templates(std::vector<std::shared_ptr<DEMMaterial>>& loaded_materials,
                                                const std::shared_ptr<DEMMaterial>& this_material);

void DEMSolver::generateJITResources() {
    /*
    // Dan and Ruochun decided not to extract unique input values.
    // Instead, we trust users: we simply store all clump template info users give.
    // So the unique-value-extractor block is disabled and commented.
    size_t input_num_clump_types = m_template_clump_mass.size();
    // Put unique clump mass values in a set.
    m_template_mass_types.insert(m_template_clump_mass.begin(), m_template_clump_mass.end());
    for (size_t i = 0; i < input_num_clump_types; i++) {
        // Put unique sphere radii values in a set.
        m_template_sp_radii_types.insert(m_template_sp_radii.at(i).begin(), m_template_sp_radii.at(i).end());
        // Put unique clump sphere component locations in a set.
        m_clumps_sp_location_types.insert(m_template_sp_relPos.at(i).begin(), m_template_sp_relPos.at(i).end());
    }
    // Now rearrange so the original input mass and sphere radii are now stored as the offsets to their respective
    // uniques sets.
    for (size_t i = 0; i < input_num_clump_types; i++) {
        m_template_mass_type_offset.push_back(
            std::distance(m_template_mass_types.begin(), m_template_mass_types.find(m_template_clump_mass.at(i))));
        std::vector<distinctSphereRadiiOffset_default_t> sp_radii_type_offset(m_template_sp_radii.at(i).size(), 0);
        std::vector<distinctSphereRelativePositions_default_t> sp_location_type_offset(
            m_template_sp_relPos.at(i).size(), 0);
        for (size_t j = 0; j < sp_radii_type_offset.size(); j++) {
            sp_radii_type_offset.at(j) = std::distance(m_template_sp_radii_types.begin(),
                                                       m_template_sp_radii_types.find(m_template_sp_radii.at(i).at(j)));
            sp_location_type_offset.at(j) =
                std::distance(m_clumps_sp_location_types.begin(),
                              m_clumps_sp_location_types.find(m_template_sp_relPos.at(i).at(j)));
        }
        m_template_sp_radii_type_offset.push_back(sp_radii_type_offset);
        m_clumps_sp_location_type_offset.push_back(sp_location_type_offset);
    }

    nDistinctClumpBodyTopologies = m_template_mass_types.size();
    nMatTuples = m_loaded_materials.size();

    nDistinctSphereRadii_computed = m_template_sp_radii_types.size();
    nDistinctSphereRelativePositions_computed = m_clumps_sp_location_types.size();
    */

    // Figure out the parameters related to the simulation `world', if need to
    if (!explicit_nv_override) {
        figureOutNV();
    }
    figureOutOrigin();
    addWorldBoundingBox();

    // Flatten cached clump templates (from ClumpTemplate structs to float arrays), make ready for transferring to kTdT
    preprocessClumpTemplates();

    // Figure out info about external objects/clump templates and whether they can be jitified
    preprocessAnalyticalObjs();

    // Count how many triangle tempaltes are there and flatten them
    preprocessTriangleObjs();

    // Process the loaded materials. The pre-process of external objects and clumps could add more materials, so this
    // call need to go after those pre-process ones.
    figureOutMaterialProxies();

    // Based on user input, prepare family_mask_matrix (family contact map matrix)
    figureOutFamilyMasks();

    // Compute stats
    nDistinctClumpBodyTopologies = m_template_clump_mass.size();
    nDistinctMassProperties = nDistinctClumpBodyTopologies + nExtObj + nTriEntities;

    // Also, external objects may introduce more material types
    nMatTuples = m_loaded_materials.size();

    // Decide bin size (for contact detection)
    decideBinSize();

    // Finally, with both user inputs and jit info processed, we can derive the number of owners that we have now
    nOwnerBodies = nExtObj + nOwnerClumps + nTriEntities;

    // If these `computed' numbers are larger than types like materialsOffset_t can hold, then we should error out and
    // let the user re-compile (or, should we somehow change the header automatically?)
    postJITResourceGenSanityCheck();

    // Notify the user how jitification goes
    reportInitStats();
}

void DEMSolver::postJITResourceGenSanityCheck() {
    // Can we jitify all clump templates?
    bool unable_jitify_all = false;
    nDistinctClumpComponents = 0;
    nJitifiableClumpComponents = 0;
    for (unsigned int i = 0; i < nDistinctClumpBodyTopologies; i++) {
        nDistinctClumpComponents += m_template_sp_radii.at(i).size();
        // Keep an eye on if the accumulated DistinctClumpComponents gets too many
        if ((!unable_jitify_all) && (nDistinctClumpComponents > DEM_THRESHOLD_CANT_JITIFY_ALL_COMP)) {
            nJitifiableClumpTopo = i;
            nJitifiableClumpComponents = nDistinctClumpComponents - m_template_sp_radii.at(i).size();
            unable_jitify_all = true;
        }
    }
    if (unable_jitify_all) {
        SGPS_DEM_WARNING(
            "There are %u clump templates loaded, but only %u templates (totalling %u components) are jitifiable due "
            "to some of the clumps are big and/or there are many types of clumps.\nIf you have external objects "
            "represented by spherical decomposition (a.k.a. intend to use big clumps), there is probably nothing to "
            "worry about.\nOtherwise, you may want to change the way this problem is formulated so you have fewer "
            "clump templates.",
            nDistinctClumpBodyTopologies, nJitifiableClumpTopo, nJitifiableClumpComponents);
    } else {
        nJitifiableClumpTopo = nDistinctClumpBodyTopologies;
        nJitifiableClumpComponents = nDistinctClumpComponents;
    }

    // Sanity check for analytical geometries
    if (nAnalGM > SGPS_DEM_THRESHOLD_TOO_MANY_ANAL_GEO) {
        SGPS_DEM_WARNING(
            "%u analytical geometries are loaded. Because all analytical geometries are jitified, this is a relatively "
            "large amount.\nIf just-in-time compilation fails or kernels run slowly, this could be a cause.",
            nAnalGM);
    }

    // Sanity check for final number of mass properties/inertia offsets
    //// TODO: Maybe mass properties should also have jitifiable and non-jitifiable part???
    if (nDistinctMassProperties >= std::numeric_limits<inertiaOffset_t>::max()) {
        SGPS_DEM_ERROR(
            "%u different mass properties (from the contribution of clump templates, analytical objects and meshed "
            "objects) are loaded, but the max allowance is %u (No.%u is reserved).\nThis many types of mass properties "
            "are not recommended but if they are indeed needed, you can redefine inertiaOffset_t.",
            nDistinctMassProperties, std::numeric_limits<inertiaOffset_t>::max() - 1,
            std::numeric_limits<inertiaOffset_t>::max());
    }

    // Do we have more bins that our data type can handle?
    if (m_num_bins > std::numeric_limits<binID_t>::max()) {
        SGPS_DEM_ERROR(
            "The simulation world has %zu bins (for domain partitioning in contact detection), but the largest bin ID "
            "that we can have is %zu.\nYou can try to make bins larger via InstructBinSize, or redefine binID_t and "
            "recompile.",
            m_num_bins, std::numeric_limits<binID_t>::max());
    }

    // Debug outputs
    SGPS_DEM_DEBUG_EXEC(printf("These owners are tracked: ");
                        for (const auto& tracked
                             : m_tracked_objs) { printf("%zu, ", tracked->ownerID); } printf("\n"););
}

void DEMSolver::preprocessClumpTemplates() {
    // A sort based on the number of components of each clump type is needed, so larger clumps are near the end of the
    // array, so we can always jitify the smaller clumps, and leave larger ones in GPU global memory
    std::sort(m_templates.begin(), m_templates.end(),
              [](auto& left, auto& right) { return left->nComp < right->nComp; });
    // A mapping is needed to transform the user-defined clump type array so that it matches the new, rearranged clump
    // template array
    std::unordered_map<inertiaOffset_t, inertiaOffset_t> old_mark_to_new;
    for (unsigned int i = 0; i < m_templates.size(); i++) {
        old_mark_to_new[m_templates.at(i)->mark] = i;
        SGPS_DEM_DEBUG_PRINTF("Clump template re-order: %u->%u, nComp: %u", m_templates.at(i)->mark, i,
                              m_templates.at(i)->nComp);
    }
    // If the user then add more clumps to the system (without adding templates, which mandates a re-initialization),
    // mapping again is not needed, because now we redefine each template's mark to be the same as their current
    // position in template array
    for (unsigned int i = 0; i < m_templates.size(); i++) {
        m_templates.at(i)->mark = i;
    }

    // Now we can flatten clump template and make ready for transfer
    for (const auto& clump : m_templates) {
        m_template_clump_mass.push_back(clump->mass);
        m_template_clump_moi.push_back(clump->MOI);
        m_template_sp_radii.push_back(clump->radii);
        // TODO: If CoM is not all-0, then relPos should be massaged here
        m_template_sp_relPos.push_back(clump->relPos);

        // m_template_sp_mat_ids is an array of ints that represent the indices of the material array
        std::vector<unsigned int> this_clump_sp_mat_ids;
        for (const std::shared_ptr<DEMMaterial>& this_material : clump->materials) {
            this_clump_sp_mat_ids.push_back(stash_material_in_templates(m_loaded_materials, this_material));
        }
        m_template_sp_mat_ids.push_back(this_clump_sp_mat_ids);
        SGPS_DEM_DEBUG_EXEC(printf("Input clump No.%d has material types: ", m_template_clump_mass.size() - 1);
                            for (unsigned int i = 0; i < this_clump_sp_mat_ids.size();
                                 i++) { printf("%d, ", this_clump_sp_mat_ids.at(i)); } printf("\n"););
    }
}

void DEMSolver::addAnalCompTemplate(const objType_t type,
                                    const std::shared_ptr<DEMMaterial>& material,
                                    const unsigned int owner,
                                    const float3 pos,
                                    const float3 rot,
                                    const float d1,
                                    const float d2,
                                    const float d3,
                                    const objNormal_t normal) {
    m_anal_types.push_back(type);
    m_anal_materials.push_back(stash_material_in_templates(m_loaded_materials, material));
    m_anal_owner.push_back(owner);
    m_anal_comp_pos.push_back(pos);
    m_anal_comp_rot.push_back(rot);
    m_anal_size_1.push_back(d1);
    m_anal_size_2.push_back(d2);
    m_anal_size_3.push_back(d3);
    m_anal_normals.push_back(normal);
}

void DEMSolver::jitifyKernels() {
    std::unordered_map<std::string, std::string> templateSubs, templateAcqSubs, simParamSubs, massMatSubs,
        familyMaskSubs, familyPrescribeSubs, familyChangesSubs, analGeoSubs, forceModelSubs;
    equipClumpTemplates(templateSubs);
    equipClumpTemplateAcquisition(templateAcqSubs);
    equipSimParams(simParamSubs);
    equipMassMat(massMatSubs);
    equipAnalGeoTemplates(analGeoSubs);
    equipFamilyMasks(familyMaskSubs);
    equipFamilyPrescribedMotions(familyPrescribeSubs);
    equipFamilyOnFlyChanges(familyChangesSubs);
    equipForceModel(forceModelSubs);
    kT->jitifyKernels(templateSubs, templateAcqSubs, simParamSubs, massMatSubs, familyMaskSubs, familyPrescribeSubs,
                      familyChangesSubs, analGeoSubs);
    dT->jitifyKernels(templateSubs, templateAcqSubs, simParamSubs, massMatSubs, familyMaskSubs, familyPrescribeSubs,
                      familyChangesSubs, analGeoSubs, forceModelSubs);
}

void DEMSolver::processUserInputs() {
    // The number of loaded clumps is calculated here, not in generateJITResources like meshes and analytical objects,
    // because clumps are not flattened before transferring to dT, so I just throw it here, somewhere eeearly in the
    // initialization process. Good idea? Also note that there is no need to initialize nOwnerClumps = 0, as
    // re-initialization may be called in mid-simulation using an `Add' flavor.
    for (const auto& a_batch : cached_input_clump_batches) {
        nOwnerClumps += a_batch->GetNumClumps();
        for (size_t i = 0; i < a_batch->GetNumClumps(); i++) {
            unsigned int nComp = a_batch->types.at(i)->nComp;
            nSpheresGM += nComp;
        }
        // Family number is flattened here, only because figureOutFamilyMasks() needs it
        m_input_clump_family.insert(m_input_clump_family.end(), a_batch->families.begin(), a_batch->families.end());
    }

    // Fix the reserved family (reserved family number is in user family, not in impl family)
    SetFamilyFixed(DEM_RESERVED_FAMILY_NUM);

    // Enlarge the expand factor if the user tells us to
    m_expand_factor *= m_expand_safety_param;
}

void DEMSolver::figureOutNV() {}

void DEMSolver::decideBinSize() {
    // find the smallest radius
    for (auto elem : m_template_sp_radii) {
        for (auto radius : elem) {
            if (radius < m_smallest_radius) {
                m_smallest_radius = radius;
            }
        }
    }

    // TODO: What should be a default bin size?
    if (m_smallest_radius > SGPS_DEM_TINY_FLOAT) {
        if (!m_use_user_instructed_bin_size) {
            m_binSize = 2.0 * m_smallest_radius;
        }
    } else {
        if (!m_use_user_instructed_bin_size) {
            SGPS_DEM_ERROR(
                "There are spheres in clump templates that have 0 radii, and the user did not specify the bin size "
                "(for contact detection)!\nBecause the bin size is supposed to be defaulted to the size of the "
                "smallest sphere, now the solver does not know what to do.");
        } else {
            SGPS_DEM_WARNING(
                "There are spheres in clump templates that have 0 radii!! Please make sure this is intentional.");
        }
    }

    nbX = (binID_t)(m_voxelSize * (double)((size_t)1 << nvXp2) / m_binSize) + 1;
    nbY = (binID_t)(m_voxelSize * (double)((size_t)1 << nvYp2) / m_binSize) + 1;
    nbZ = (binID_t)(m_voxelSize * (double)((size_t)1 << nvZp2) / m_binSize) + 1;
    m_num_bins = (uint64_t)nbX * (uint64_t)nbY * (uint64_t)nbZ;
    // It's better to compute num of bins this way, rather than...
    // (uint64_t)(m_boxX / m_binSize + 1) * (uint64_t)(m_boxY / m_binSize + 1) * (uint64_t)(m_boxZ / m_binSize + 1);
    // because the space bins and voxels can cover may be larger than the user-defined sim domain
}

inline void DEMSolver::reportInitStats() const {
    SGPS_DEM_INFO("The dimension of the simulation world: %.17g, %.17g, %.17g", m_boxX, m_boxY, m_boxZ);
    SGPS_DEM_INFO("Simulation world X range: [%.7g, %.7g]", m_boxLBF.x, m_boxLBF.x + m_boxX);
    SGPS_DEM_INFO("Simulation world Y range: [%.7g, %.7g]", m_boxLBF.y, m_boxLBF.y + m_boxY);
    SGPS_DEM_INFO("Simulation world Z range: [%.7g, %.7g]", m_boxLBF.z, m_boxLBF.z + m_boxZ);
    SGPS_DEM_INFO("User-specified dimensions are not larger than the above simulation world.");
    SGPS_DEM_INFO("User-specified X-dimension range: [%.7g, %.7g]", m_boxLBF.x, m_boxLBF.x + m_user_boxSize.x);
    SGPS_DEM_INFO("User-specified Y-dimension range: [%.7g, %.7g]", m_boxLBF.y, m_boxLBF.y + m_user_boxSize.y);
    SGPS_DEM_INFO("User-specified Z-dimension range: [%.7g, %.7g]", m_boxLBF.z, m_boxLBF.z + m_user_boxSize.z);
    SGPS_DEM_INFO("The length unit in this simulation is: %.17g", l);
    SGPS_DEM_INFO("The edge length of a voxel: %.17g", m_voxelSize);

    SGPS_DEM_INFO("The edge length of a bin: %.17g", m_binSize);
    SGPS_DEM_INFO("The total number of bins: %zu", m_num_bins);

    SGPS_DEM_INFO("The total number of clumps: %zu", nOwnerClumps);
    SGPS_DEM_INFO("The combined number of component spheres: %zu", nSpheresGM);
    SGPS_DEM_INFO("The total number of analytical objects: %u", nExtObj);
    SGPS_DEM_INFO("Grand total number of owners: %zu", nOwnerBodies);
    SGPS_DEM_INFO("The total number of families: %u", nDistinctFamilies);

    if (m_expand_factor > 0.0) {
        SGPS_DEM_INFO("All geometries are enlarged/thickened by %.9g for contact detection purpose", m_expand_factor);
        SGPS_DEM_INFO("This in the case of the smallest sphere, means enlarging radius by %.9g%%",
                      (m_expand_factor / m_smallest_radius) * 100.0);
    }

    SGPS_DEM_INFO("The number of material types: %u", nMatTuples);
    if (m_isHistoryless) {
        SGPS_DEM_INFO("This run uses HISTORYLESS solver setup");
    } else {
        SGPS_DEM_INFO("This run uses HISTORY-BASED solver setup");
    }
    // TODO: The solver model, is it user-specified or internally defined?
}

void DEMSolver::preprocessAnalyticalObjs() {
    // nExtObj can increase in mid-simulation if the user re-initialize using an `Add' flavor
    nExtObj += cached_extern_objs.size();
    unsigned int thisExtObj = 0;
    for (const auto& ext_obj : cached_extern_objs) {
        // Load mass and MOI properties into arrays waiting to be transfered to kTdT
        m_ext_obj_mass.push_back(ext_obj->mass);
        m_ext_obj_moi.push_back(ext_obj->MOI);

        //// TODO: If CoM is not all-0, all components should be offsetted
        // float3 CoM = ext_obj->CoM;
        // float4 CoM_oriQ = ext_obj->CoM_oriQ;

        // Then load this ext obj's components
        unsigned int this_num_anal_ent = 0;
        auto comp_params = ext_obj->entity_params;
        auto comp_mat = ext_obj->materials;
        m_input_ext_obj_xyz.push_back(ext_obj->init_pos);
        //// TODO: init_oriQ?????
        m_input_ext_obj_family.push_back(ext_obj->family_code);
        for (unsigned int i = 0; i < ext_obj->types.size(); i++) {
            auto param = comp_params.at(this_num_anal_ent);
            this_num_anal_ent++;
            switch (ext_obj->types.at(i)) {
                case DEM_OBJ_COMPONENT::PLANE:
                    addAnalCompTemplate(DEM_ENTITY_TYPE_PLANE, comp_mat.at(i), thisExtObj, param.plane.position,
                                        param.plane.normal);
                    break;
                case DEM_OBJ_COMPONENT::PLATE:
                    addAnalCompTemplate(DEM_ENTITY_TYPE_PLATE, comp_mat.at(i), thisExtObj, param.plate.center,
                                        param.plate.normal, param.plate.h_dim_x, param.plate.h_dim_y);
                    break;
                default:
                    SGPS_DEM_ERROR("There is at least one analytical boundary that has a type not supported.");
            }
        }
        nAnalGM += this_num_anal_ent;
        thisExtObj++;
    }
}

void DEMSolver::preprocessTriangleObjs() {
    nTriEntities += cached_mesh_objs.size();
    unsigned int thisMeshObj = 0;
    for (const auto& mesh_obj : cached_mesh_objs) {
        m_mesh_obj_mass.push_back(mesh_obj->mass);
        m_mesh_obj_moi.push_back(mesh_obj->MOI);
        //// TODO: If CoM is not all-0, all components should be offsetted
        // float3 CoM = ext_obj->CoM;
        // float4 CoM_oriQ = ext_obj->CoM_oriQ;

        m_input_mesh_obj_xyz.push_back(mesh_obj->init_pos);
        m_input_mesh_obj_rot.push_back(mesh_obj->init_oriQ);
        m_input_mesh_obj_family.push_back(mesh_obj->family_code);
        m_mesh_facet_owner.insert(m_mesh_facet_owner.end(), mesh_obj->GetNumTriangles(), thisMeshObj);
        for (unsigned int i = 0; i < mesh_obj->GetNumTriangles(); i++) {
            m_mesh_facet_materials.push_back(
                stash_material_in_templates(m_loaded_materials, mesh_obj->materials.at(i)));
            DEMTriangle tri = mesh_obj->GetTriangle(i);
            // If we wish to correct surface orientation based on given vertex normals, rather than using RHR...
            if (mesh_obj->use_mesh_normals) {
                int normal_i = mesh_obj->face_n_indices.at(i).x;  // normals at each vertex of this triangle
                float3 normal = mesh_obj->normals.at(normal_i);

                // Generate normal using RHR from nodes 1, 2, and 3
                float3 AB = tri.p2 - tri.p1;
                float3 AC = tri.p3 - tri.p1;
                float3 cross_product = cross(AB, AC);

                // If the normal created by a RHR traversal is not correct, switch two vertices
                if (dot(cross_product, normal) < 0) {
                    float3 tmp = tri.p2;
                    tri.p2 = tri.p3;
                    tri.p3 = tmp;
                }
            }
            m_mesh_facets.push_back(tri);
        }

        nTriGM += mesh_obj->GetNumTriangles();
        thisMeshObj++;
    }
}

void DEMSolver::figureOutMaterialProxies() {
    // Use the info in m_loaded_materials to populate API-side proxy arrays
    // These arrays are later passed to kTdT in initManagedArrays
    unsigned int count = m_loaded_materials.size();
    m_E_proxy.resize(count);
    m_nu_proxy.resize(count);
    m_CoR_proxy.resize(count);
    m_mu_proxy.resize(count);
    m_Crr_proxy.resize(count);
    for (unsigned int i = 0; i < count; i++) {
        std::shared_ptr<DEMMaterial>& Mat = m_loaded_materials.at(i);
        m_E_proxy.at(i) = Mat->E;
        m_nu_proxy.at(i) = Mat->nu;
        m_CoR_proxy.at(i) = Mat->CoR;
        m_mu_proxy.at(i) = Mat->mu;
        m_Crr_proxy.at(i) = Mat->Crr;
    }
}

void DEMSolver::figureOutFamilyMasks() {
    // Figure out the unique family numbers
    std::vector<unsigned int> unique_clump_families = hostUniqueVector<unsigned int>(m_input_clump_family);
    if (any_of(unique_clump_families.begin(), unique_clump_families.end(),
               [](unsigned int i) { return i >= DEM_RESERVED_FAMILY_NUM; })) {
        SGPS_DEM_WARNING(
            "Some clumps are instructed to have family number %u (or larger).\nThis family number is reserved for "
            "completely fixed boundaries. Using it on your simulation entities will make them fixed, regardless of "
            "your specification.\nYou can change family_t if you indeed need more families to work with.",
            DEM_RESERVED_FAMILY_NUM);
    }

    std::vector<unsigned int> unique_ext_obj_families = hostUniqueVector<unsigned int>(m_input_ext_obj_family);
    // TODO: find the uniques for triangle input families as well
    unique_clump_families.insert(unique_clump_families.end(), unique_ext_obj_families.begin(),
                                 unique_ext_obj_families.end());
    // Combine all unique user family numbers together
    unique_clump_families.insert(unique_clump_families.end(), unique_user_families.begin(), unique_user_families.end());
    std::vector<unsigned int> unique_families_this_time = hostUniqueVector<unsigned int>(unique_clump_families);
    unique_user_families.assign(unique_families_this_time.begin(), unique_families_this_time.end());
    unsigned int max_family_num = *(std::max_element(unique_user_families.begin(), unique_user_families.end()));

    SGPS_DEM_DEBUG_EXEC(printf("Unique user families:\n"); for (unsigned int i = 0; i < unique_user_families.size();
                                                                i++) printf("%u, ", unique_user_families.at(i));
                        printf("\n"););

    nDistinctFamilies = unique_user_families.size();
    if (nDistinctFamilies > std::numeric_limits<family_t>::max()) {
        SGPS_DEM_ERROR(
            "You have %u families, however per data type restriction, there can be no more than %u. If so many "
            "families are indeed needed, please redefine family_t.",
            nDistinctFamilies, std::numeric_limits<family_t>::max());
    }
    // displayArray<unsigned int>(unique_user_families.data(), unique_user_families.size());

    // Build the user--internal family number map (user can define family number however they want, but our
    // implementation-level numbers always start at 0)
    for (family_t i = 0; i < nDistinctFamilies; i++) {
        m_family_user_impl_map[unique_user_families.at(i)] = i;
        m_family_impl_user_map[i] = unique_user_families.at(i);
    }

    // At this point, we know the size of the mask matrix, and we init it as all-allow
    m_family_mask_matrix.resize((nDistinctFamilies + 1) * nDistinctFamilies / 2, DEM_DONT_PREVENT_CONTACT);

    // Then we figure out the masks
    for (const auto& a_pair : m_input_no_contact_pairs) {
        // Convert user-input pairs into impl-level pairs
        unsigned int implID1 = m_family_user_impl_map.at(a_pair.ID1);
        unsigned int implID2 = m_family_user_impl_map.at(a_pair.ID2);
        // Now fill in the mask matrix
        unsigned int posInMat = locateMaskPair<unsigned int>(implID1, implID2);
        m_family_mask_matrix.at(posInMat) = DEM_PREVENT_CONTACT;
    }
    // displayArray<notStupidBool_t>(m_family_mask_matrix.data(), m_family_mask_matrix.size());

    // Then, figure out each family's prescription info and put it into an (impl family number-based) array
    // Multiple user prescription input entries can work on the same array entry
    m_unique_family_prescription.resize(nDistinctFamilies);
    for (const auto& preInfo : m_input_family_prescription) {
        unsigned int user_family = preInfo.family;
        if (m_family_user_impl_map.find(user_family) == m_family_user_impl_map.end()) {
            if (user_family != DEM_RESERVED_FAMILY_NUM) {
                SGPS_DEM_WARNING(
                    "Family number %u is instructed to have prescribed motion, but no entity is associated with this "
                    "family.",
                    user_family);
            }
            continue;
        }

        auto& this_family_info = m_unique_family_prescription.at(m_family_user_impl_map.at(user_family));

        this_family_info.used = true;
        this_family_info.family = m_family_user_impl_map.at(user_family);
        if (preInfo.linPosX != "none")
            this_family_info.linPosX = preInfo.linPosX;
        if (preInfo.linPosY != "none")
            this_family_info.linPosY = preInfo.linPosY;
        if (preInfo.linPosZ != "none")
            this_family_info.linPosZ = preInfo.linPosZ;
        if (preInfo.oriQ != "none")
            this_family_info.oriQ = preInfo.oriQ;
        if (preInfo.linVelX != "none")
            this_family_info.linVelX = preInfo.linVelX;
        if (preInfo.linVelY != "none")
            this_family_info.linVelY = preInfo.linVelY;
        if (preInfo.linVelZ != "none")
            this_family_info.linVelZ = preInfo.linVelZ;
        if (preInfo.rotVelX != "none")
            this_family_info.rotVelX = preInfo.rotVelX;
        if (preInfo.rotVelY != "none")
            this_family_info.rotVelY = preInfo.rotVelY;
        if (preInfo.rotVelZ != "none")
            this_family_info.rotVelZ = preInfo.rotVelZ;
        this_family_info.linVelPrescribed = this_family_info.linVelPrescribed || preInfo.linVelPrescribed;
        this_family_info.rotVelPrescribed = this_family_info.rotVelPrescribed || preInfo.rotVelPrescribed;
        this_family_info.rotPosPrescribed = this_family_info.rotPosPrescribed || preInfo.rotPosPrescribed;
        this_family_info.linPosPrescribed = this_family_info.linPosPrescribed || preInfo.linPosPrescribed;

        this_family_info.externPos = this_family_info.externPos || preInfo.externPos;
        this_family_info.externVel = this_family_info.externVel || preInfo.externVel;

        SGPS_DEM_DEBUG_PRINTF("User family %u has prescribed lin vel: %s, %s, %s", user_family,
                              this_family_info.linVelX.c_str(), this_family_info.linVelY.c_str(),
                              this_family_info.linVelZ.c_str());
        SGPS_DEM_DEBUG_PRINTF("User family %u has prescribed ang vel: %s, %s, %s", user_family,
                              this_family_info.rotVelX.c_str(), this_family_info.rotVelY.c_str(),
                              this_family_info.rotVelZ.c_str());
    }
}

void DEMSolver::figureOutOrigin() {
    if (m_user_instructed_origin == "explicit") {
        return;
    }
    float3 O;
    if (m_user_instructed_origin == "center") {
        O = -(m_user_boxSize) / 2.0;
        m_boxLBF = O;
    } else {
        SGPS_DEM_ERROR("Unrecognized location of system origin.");
    }
}

void DEMSolver::addWorldBoundingBox() {
    // Now, add the bounding box for the simulation `world' if instructed.
    // Note the positions to add these planes are determined by the user-wanted box sizes, not m_boxXYZ which is the max
    // possible box size.
    if (m_user_add_bounding_box == "all" || m_user_add_bounding_box == "top_open") {
        auto box = this->AddExternalObject();
        box->AddPlane(
            host_make_float3(m_boxLBF.x + m_user_boxSize.x / 2., m_boxLBF.y + m_user_boxSize.y / 2., m_boxLBF.z),
            host_make_float3(0, 0, 1), m_bounding_box_material);
        box->AddPlane(
            host_make_float3(m_boxLBF.x, m_boxLBF.y + m_user_boxSize.y / 2., m_boxLBF.z + m_user_boxSize.z / 2.),
            host_make_float3(1, 0, 0), m_bounding_box_material);
        box->AddPlane(host_make_float3(m_boxLBF.x + m_user_boxSize.x, m_boxLBF.y + m_user_boxSize.y / 2.,
                                       m_boxLBF.z + m_user_boxSize.z / 2.),
                      host_make_float3(-1, 0, 0), m_bounding_box_material);
        box->AddPlane(
            host_make_float3(m_boxLBF.x + m_user_boxSize.x / 2., m_boxLBF.y, m_boxLBF.z + m_user_boxSize.z / 2.),
            host_make_float3(0, 1, 0), m_bounding_box_material);
        box->AddPlane(host_make_float3(m_boxLBF.x + m_user_boxSize.x / 2., m_boxLBF.y + m_user_boxSize.y,
                                       m_boxLBF.z + m_user_boxSize.z / 2.),
                      host_make_float3(0, -1, 0), m_bounding_box_material);
        if (m_user_add_bounding_box == "all") {
            box->AddPlane(host_make_float3(m_boxLBF.x + m_user_boxSize.x / 2., m_boxLBF.y + m_user_boxSize.y / 2.,
                                           m_boxLBF.z + m_user_boxSize.z),
                          host_make_float3(0, 0, -1), m_bounding_box_material);
        }
    }
}

// This is generally used to pass individual instructions on how the solver should behave
void DEMSolver::transferSolverParams() {
    kT->verbosity = verbosity;
    dT->verbosity = verbosity;

    // I/O policies (only output content matters for worker threads)
    dT->solverFlags.outputFlags = m_out_content;

    // Transfer historyless-ness
    kT->solverFlags.isHistoryless = m_isHistoryless;
    dT->solverFlags.isHistoryless = m_isHistoryless;

    // Tell kT and dT if this run is async
    kT->solverFlags.isAsync = !(m_updateFreq == 0);
    dT->solverFlags.isAsync = !(m_updateFreq == 0);
    // Make sure dT kT understand the lock--waiting policy of this run
    dTkT_InteractionManager->dynamicRequestedUpdateFrequency = m_updateFreq;

    // Tell kT and dT whether the user enforeced potential on-the-fly family number changes
    kT->solverFlags.canFamilyChange = m_famnum_change_conditionally;
    dT->solverFlags.canFamilyChange = m_famnum_change_conditionally;

    kT->solverFlags.should_sort_pairs = kT_should_sort;

    // NOTE: compact force calculation (in the hope to use shared memory) is not implemented
    kT->solverFlags.use_compact_force_kernel = use_compact_sweep_force_strat;
}

void DEMSolver::transferSimParams() {
    dT->setSimParams(nvXp2, nvYp2, nvZp2, l, m_voxelSize, m_binSize, nbX, nbY, nbZ, m_boxLBF, G, m_ts_size,
                     m_expand_factor);
    kT->setSimParams(nvXp2, nvYp2, nvZp2, l, m_voxelSize, m_binSize, nbX, nbY, nbZ, m_boxLBF, G, m_ts_size,
                     m_expand_factor);
}

void DEMSolver::initializeArrays() {
    // Resize managed arrays based on the statistical data we had from the previous step
    dT->allocateManagedArrays(nOwnerBodies, nOwnerClumps, nExtObj, nTriEntities, nSpheresGM, nTriGM, nAnalGM,
                              nDistinctMassProperties, nDistinctClumpBodyTopologies, nDistinctClumpComponents,
                              nJitifiableClumpComponents, nMatTuples);
    kT->allocateManagedArrays(nOwnerBodies, nOwnerClumps, nExtObj, nTriEntities, nSpheresGM, nTriGM, nAnalGM,
                              nDistinctMassProperties, nDistinctClumpBodyTopologies, nDistinctClumpComponents,
                              nJitifiableClumpComponents, nMatTuples);

    // Now we can feed those GPU-side arrays with the cached API-level simulation info
    dT->initManagedArrays(
        // Clump batchs' initial stats
        cached_input_clump_batches,
        // Analytical objects' initial stats
        m_input_ext_obj_xyz, m_input_ext_obj_family,
        // Meshed objects' initial stats
        m_input_mesh_obj_xyz, m_input_mesh_obj_rot, m_input_mesh_obj_family, m_mesh_facet_owner, m_mesh_facet_materials,
        m_mesh_facets,
        // Family number mapping
        m_family_user_impl_map, m_family_impl_user_map,
        // Clump template info (mass, sphere components, materials etc.)
        m_template_sp_mat_ids, m_template_clump_mass, m_template_clump_moi, m_template_sp_radii, m_template_sp_relPos,
        // Analytical obj `template' properties
        m_ext_obj_mass, m_ext_obj_moi,
        // Meshed obj `template' properties
        m_mesh_obj_mass, m_mesh_obj_moi,
        // Universal template info
        m_loaded_materials,
        // I/O and misc.
        m_no_output_families, m_tracked_objs);
    kT->initManagedArrays(
        // Clump batchs' initial stats
        cached_input_clump_batches,
        // Analytical objects' initial stats
        m_input_ext_obj_family,
        // Meshed objects' initial stats
        m_input_mesh_obj_family,
        // Templates and misc.
        m_family_user_impl_map, m_template_clump_mass, m_template_sp_radii, m_template_sp_relPos);
}

void DEMSolver::packDataPointers() {
    dT->packDataPointers();
    kT->packDataPointers();
    // Each worker thread needs pointers used for data transfering. Note this step must be done after packDataPointers
    // are called, so each thread has its own pointers packed.
    dT->packTransferPointers(kT);
    kT->packTransferPointers(dT);
}

void DEMSolver::validateUserInputs() {
    //// TODO: Remove this constraint
    if (m_loaded_materials.size() == 0) {
        SGPS_DEM_ERROR(
            "Before initializing the system, at least one material type should be loaded via LoadMaterialType.");
    }
    if (m_ts_size <= 0.0 && ts_size_is_const) {
        SGPS_DEM_ERROR(
            "Time step size is set to be %f. Please supply a positive number via SetTimeStepSize, or define the "
            "variable stepping properly.",
            m_ts_size);
    }
    if (m_templates.size() == 0) {
        SGPS_DEM_ERROR("Before initializing the system, at least one clump type should be defined via LoadClumpType.");
    }

    if (m_user_boxSize.x <= 0.f || m_user_boxSize.y <= 0.f || m_user_boxSize.z <= 0.f) {
        SGPS_DEM_ERROR(
            "The size of the simulation world is set to be (or default to be) %f by %f by %f. It is impossibly small.",
            m_user_boxSize.x, m_user_boxSize.y, m_user_boxSize.z);
    }

    if (m_expand_factor * m_expand_safety_param <= 0.0 && m_updateFreq > 0) {
        SGPS_DEM_WARNING(
            "You instructed that the physics can stretch %u time steps into the future, but did not instruct the "
            "geometries to expand via SuggestExpandFactor. The contact detection procedure will likely fail to detect "
            "some contact events before it is too late, hindering the simulation accuracy and stability.",
            m_updateFreq);
    }
    if (m_updateFreq < 0) {
        SGPS_DEM_WARNING(
            "The physics of the DEM system can drift into the future as much as it wants compared to contact "
            "detections, because SetCDUpdateFreq was called with a negative argument. Please make sure this is "
            "intended.");
    }

    if (m_user_defined_force_model) {
        // TODO: See if this user model makes sense
    }
}

// TODO: it seems that for variable step size, it is the best not to do the computation of n cycles here; rather we
// should use a while loop to control that loop in worker threads.
size_t DEMSolver::computeDTCycles(double thisCallDuration) {
    return (size_t)std::round(thisCallDuration / m_ts_size);
}

// Test if 2 types of DEM materials are the same
inline bool is_DEM_material_same(const std::shared_ptr<DEMMaterial>& a, const std::shared_ptr<DEMMaterial>& b) {
    if (std::abs(a->E - b->E) > SGPS_DEM_TINY_FLOAT) {
        return false;
    }
    if (std::abs(a->nu - b->nu) > SGPS_DEM_TINY_FLOAT) {
        return false;
    }
    if (std::abs(a->CoR - b->CoR) > SGPS_DEM_TINY_FLOAT) {
        return false;
    }
    if (std::abs(a->mu - b->mu) > SGPS_DEM_TINY_FLOAT) {
        return false;
    }
    if (std::abs(a->Crr - b->Crr) > SGPS_DEM_TINY_FLOAT) {
        return false;
    }
    return true;
}

/// Check if this_material is in loaded_materials: if yes, return the correspnding index in loaded_materials; if not,
/// load it and return the correspnding index in loaded_materials (the last element)
inline unsigned int stash_material_in_templates(std::vector<std::shared_ptr<DEMMaterial>>& loaded_materials,
                                                const std::shared_ptr<DEMMaterial>& this_material) {
    auto is_same = [&](const std::shared_ptr<DEMMaterial>& ptr) { return is_DEM_material_same(ptr, this_material); };
    // Is this material already loaded? (most likely yes)
    auto it_mat = std::find_if(loaded_materials.begin(), loaded_materials.end(), is_same);
    if (it_mat != loaded_materials.end()) {
        // Already in, then just get where it's located in the m_loaded_materials array
        return std::distance(loaded_materials.begin(), it_mat);
    } else {
        // Not already in, come on. Load it, and then get it into this_clump_sp_mat_ids. This is unlikely, unless the
        // users made a shared_ptr themselves.
        loaded_materials.push_back(this_material);
        return loaded_materials.size() - 1;
    }
}

inline void DEMSolver::equipForceModel(std::unordered_map<std::string, std::string>& strMap) {
    std::string model = m_force_model;
    if (m_ensure_kernel_line_num) {
        std::string model = compact_code(model);
    }
    strMap["_DEMForceModel_"] = model;
}

inline void DEMSolver::equipFamilyOnFlyChanges(std::unordered_map<std::string, std::string>& strMap) {
    std::string condStr = " ";
    unsigned int n_rules = m_family_change_pairs.size();
    for (unsigned int i = 0; i < n_rules; i++) {
        // User family num and internal family num are not the same
        // Convert user-input pairs into impl-level pairs
        unsigned int implID1 = m_family_user_impl_map.at(m_family_change_pairs.at(i).ID1);
        unsigned int implID2 = m_family_user_impl_map.at(m_family_change_pairs.at(i).ID2);

        // The conditions will be handled by a series of if statements
        std::string cond = "if (family_code == " + std::to_string(implID1) + ") { bool shouldMakeChange = false;";
        std::string user_str = replace_pattern(m_family_change_conditions.at(i), "return", "shouldMakeChange = ");
        if (m_ensure_kernel_line_num) {
            user_str = compact_code(user_str);
        }
        cond += user_str;
        cond += "if (shouldMakeChange) {granData->familyID[thisClump] = " + std::to_string(implID2) + ";}";
        cond += "}";
        condStr += cond;
    }

    strMap["_nRulesOfChange_"] = std::to_string(n_rules);
    strMap["_familyChangeRules_"] = condStr;
}

inline void DEMSolver::equipFamilyPrescribedMotions(std::unordered_map<std::string, std::string>& strMap) {
    std::string velStr = " ", posStr = " ";
    for (const auto& preInfo : m_unique_family_prescription) {
        if (!preInfo.used) {
            continue;
        }
        velStr += "case " + std::to_string(preInfo.family) + ": {";
        posStr += "case " + std::to_string(preInfo.family) + ": {";
        if (!preInfo.externVel) {
            if (preInfo.linVelX != "none")
                velStr += "vX = " + preInfo.linVelX + ";";
            if (preInfo.linVelY != "none")
                velStr += "vY = " + preInfo.linVelY + ";";
            if (preInfo.linVelZ != "none")
                velStr += "vZ = " + preInfo.linVelZ + ";";
            if (preInfo.rotVelX != "none")
                velStr += "omgBarX = " + preInfo.rotVelX + ";";
            if (preInfo.rotVelY != "none")
                velStr += "omgBarY = " + preInfo.rotVelY + ";";
            if (preInfo.rotVelZ != "none")
                velStr += "omgBarZ = " + preInfo.rotVelZ + ";";
            velStr += "LinPrescribed = " + std::to_string(preInfo.linVelPrescribed) + ";";
            velStr += "RotPrescribed = " + std::to_string(preInfo.rotVelPrescribed) + ";";
        }  // TODO: add externVel==True case, loading from external vectors
        velStr += "break; }";
        if (!preInfo.externPos) {
            if (preInfo.linPosX != "none")
                posStr += "X = " + preInfo.linPosX + ";";
            if (preInfo.linPosY != "none")
                posStr += "Y = " + preInfo.linPosY + ";";
            if (preInfo.linPosZ != "none")
                posStr += "Z = " + preInfo.linPosZ + ";";
            if (preInfo.oriQ != "none") {
                posStr += "float4 myOriQ = " + preInfo.oriQ + ";";
                posStr += "ori0 = myOriQ.x; ori1 = myOriQ.y; ori2 = myOriQ.z; ori3 = myOriQ.w;";
            }
            posStr += "LinPrescribed = " + std::to_string(preInfo.linPosPrescribed) + ";";
            posStr += "RotPrescribed = " + std::to_string(preInfo.rotPosPrescribed) + ";";
        }  // TODO: add externPos==True case, loading from external vectors
        posStr += "break; }";
    }
    strMap["_velPrescriptionStrategy_"] = velStr;
    strMap["_posPrescriptionStrategy_"] = posStr;
}

inline void DEMSolver::equipFamilyMasks(std::unordered_map<std::string, std::string>& strMap) {
    std::string maskMat;
    strMap["_nFamilyMaskEntries_"] = std::to_string(m_family_mask_matrix.size());
    for (unsigned int i = 0; i < m_family_mask_matrix.size(); i++) {
        maskMat += std::to_string(m_family_mask_matrix.at(i)) + ",";
    }
    strMap["_familyMasks_"] = maskMat;
}

inline void DEMSolver::equipAnalGeoTemplates(std::unordered_map<std::string, std::string>& strMap) {
    // Some sim systems can have 0 boundary entities in them. In this case, we have to ensure jitification does not fail
    std::string objOwner = " ", objType = " ", objMat = " ", objNormal = " ", objRelPosX = " ", objRelPosY = " ",
                objRelPosZ = " ", objRotX = " ", objRotY = " ", objRotZ = " ", objSize1 = " ", objSize2 = " ",
                objSize3 = " ";
    for (unsigned int i = 0; i < nAnalGM; i++) {
        // External objects will be owners, and their IDs are following template-loaded simulation clumps
        bodyID_t myOwner = nOwnerClumps + m_anal_owner.at(i);
        objOwner += std::to_string(myOwner) + ",";
        objType += std::to_string(m_anal_types.at(i)) + ",";
        objMat += std::to_string(m_anal_materials.at(i)) + ",";
        objNormal += std::to_string(m_anal_normals.at(i)) + ",";
        objRelPosX += to_string_with_precision(m_anal_comp_pos.at(i).x) + ",";
        objRelPosY += to_string_with_precision(m_anal_comp_pos.at(i).y) + ",";
        objRelPosZ += to_string_with_precision(m_anal_comp_pos.at(i).z) + ",";
        objRotX += to_string_with_precision(m_anal_comp_rot.at(i).x) + ",";
        objRotY += to_string_with_precision(m_anal_comp_rot.at(i).y) + ",";
        objRotZ += to_string_with_precision(m_anal_comp_rot.at(i).z) + ",";
        objSize1 += to_string_with_precision(m_anal_size_1.at(i)) + ",";
        objSize2 += to_string_with_precision(m_anal_size_2.at(i)) + ",";
        objSize3 += to_string_with_precision(m_anal_size_3.at(i)) + ",";
    }

    strMap["_objOwner_"] = objOwner;
    strMap["_objType_"] = objType;
    strMap["_objMaterial_"] = objMat;
    strMap["_objNormal_"] = objNormal;

    strMap["_objRelPosX_"] = objRelPosX;
    strMap["_objRelPosY_"] = objRelPosY;
    strMap["_objRelPosZ_"] = objRelPosZ;

    strMap["_objRotX_"] = objRotX;
    strMap["_objRotY_"] = objRotY;
    strMap["_objRotZ_"] = objRotZ;

    strMap["_objSize1_"] = objSize1;
    strMap["_objSize2_"] = objSize2;
    strMap["_objSize3_"] = objSize3;
}

inline void DEMSolver::equipMassMat(std::unordered_map<std::string, std::string>& strMap) {
    std::string MassProperties, moiX, moiY, moiZ, E_proxy, nu_proxy, CoR_proxy, mu_proxy, Crr_proxy;
    // Loop through all templates to jitify them
    for (unsigned int i = 0; i < m_template_clump_mass.size(); i++) {
        MassProperties += to_string_with_precision(m_template_clump_mass.at(i)) + ",";
        moiX += to_string_with_precision(m_template_clump_moi.at(i).x) + ",";
        moiY += to_string_with_precision(m_template_clump_moi.at(i).y) + ",";
        moiZ += to_string_with_precision(m_template_clump_moi.at(i).z) + ",";
    }
    for (unsigned int i = 0; i < m_ext_obj_mass.size(); i++) {
        MassProperties += to_string_with_precision(m_ext_obj_mass.at(i)) + ",";
        moiX += to_string_with_precision(m_ext_obj_moi.at(i).x) + ",";
        moiY += to_string_with_precision(m_ext_obj_moi.at(i).y) + ",";
        moiZ += to_string_with_precision(m_ext_obj_moi.at(i).z) + ",";
    }
    for (unsigned int i = 0; i < m_mesh_obj_mass.size(); i++) {
        MassProperties += to_string_with_precision(m_mesh_obj_mass.at(i)) + ",";
        moiX += to_string_with_precision(m_mesh_obj_moi.at(i).x) + ",";
        moiY += to_string_with_precision(m_mesh_obj_moi.at(i).y) + ",";
        moiZ += to_string_with_precision(m_mesh_obj_moi.at(i).z) + ",";
    }
    for (unsigned int i = 0; i < nMatTuples; i++) {
        E_proxy += to_string_with_precision(m_E_proxy.at(i)) + ",";
        nu_proxy += to_string_with_precision(m_nu_proxy.at(i)) + ",";
        CoR_proxy += to_string_with_precision(m_CoR_proxy.at(i)) + ",";
        mu_proxy += to_string_with_precision(m_mu_proxy.at(i)) + ",";
        Crr_proxy += to_string_with_precision(m_Crr_proxy.at(i)) + ",";
    }
    strMap["_MassProperties_"] = MassProperties;
    strMap["_moiX_"] = moiX;
    strMap["_moiY_"] = moiY;
    strMap["_moiZ_"] = moiZ;
    strMap["_EProxy_"] = E_proxy;
    strMap["_nuProxy_"] = nu_proxy;
    strMap["_CoRProxy_"] = CoR_proxy;
    strMap["_muProxy_"] = mu_proxy;
    strMap["_CrrProxy_"] = Crr_proxy;
}

inline void DEMSolver::equipClumpTemplateAcquisition(std::unordered_map<std::string, std::string>& strMap) {
    // This part is different depending on whether we have clump templates that are in global memory only
    std::string componentAcqStrat;
    if (nJitifiableClumpTopo == nDistinctClumpBodyTopologies) {
        // In this case, all clump templates can be jitified
        componentAcqStrat = DEM_CLUMP_COMPONENT_ACQUISITION_ALL_JITIFIED();
    } else if (nJitifiableClumpTopo < nDistinctClumpBodyTopologies) {
        // In this case, some clump templates are in the global memory
        componentAcqStrat = DEM_CLUMP_COMPONENT_ACQUISITION_PARTIALLY_JITIFIED();
    }
    if (m_ensure_kernel_line_num) {
        componentAcqStrat = compact_code(componentAcqStrat);
    }
    strMap["_componentAcqStrat_"] = componentAcqStrat;
}

inline void DEMSolver::equipClumpTemplates(std::unordered_map<std::string, std::string>& strMap) {
    std::string CDRadii, Radii, CDRelPosX, CDRelPosY, CDRelPosZ;
    // Loop through all clump templates to jitify them, but without going over the shared memory limit
    for (unsigned int i = 0; i < nJitifiableClumpTopo; i++) {
        for (unsigned int j = 0; j < m_template_sp_radii.at(i).size(); j++) {
            Radii += to_string_with_precision(m_template_sp_radii.at(i).at(j)) + ",";
            CDRadii += to_string_with_precision(m_template_sp_radii.at(i).at(j) + m_expand_factor) + ",";
            CDRelPosX += to_string_with_precision(m_template_sp_relPos.at(i).at(j).x) + ",";
            CDRelPosY += to_string_with_precision(m_template_sp_relPos.at(i).at(j).y) + ",";
            CDRelPosZ += to_string_with_precision(m_template_sp_relPos.at(i).at(j).z) + ",";
        }
    }
    strMap["_Radii_"] = Radii;
    strMap["_CDRadii_"] = CDRadii;
    strMap["_CDRelPosX_"] = CDRelPosX;
    strMap["_CDRelPosY_"] = CDRelPosY;
    strMap["_CDRelPosZ_"] = CDRelPosZ;
}

inline void DEMSolver::equipSimParams(std::unordered_map<std::string, std::string>& strMap) {
    strMap["_nvXp2_"] = std::to_string(nvXp2);
    strMap["_nvYp2_"] = std::to_string(nvYp2);
    strMap["_nvZp2_"] = std::to_string(nvZp2);

    strMap["_nbX_"] = std::to_string(nbX);
    strMap["_nbY_"] = std::to_string(nbY);
    strMap["_nbZ_"] = std::to_string(nbZ);

    strMap["_l_"] = to_string_with_precision(l);
    strMap["_voxelSize_"] = to_string_with_precision(m_voxelSize);
    strMap["_binSize_"] = to_string_with_precision(m_binSize);

    strMap["_nAnalGM_"] = std::to_string(nAnalGM);
    strMap["_nOwnerBodies_"] = std::to_string(nOwnerBodies);
    strMap["_nSpheresGM_"] = std::to_string(nSpheresGM);

    strMap["_LBFX_"] = to_string_with_precision(m_boxLBF.x);
    strMap["_LBFY_"] = to_string_with_precision(m_boxLBF.y);
    strMap["_LBFZ_"] = to_string_with_precision(m_boxLBF.z);
    strMap["_Gx_"] = to_string_with_precision(G.x);
    strMap["_Gy_"] = to_string_with_precision(G.y);
    strMap["_Gz_"] = to_string_with_precision(G.z);
    strMap["_beta_"] = to_string_with_precision(m_expand_factor);

    // Some constants that we should consider using or not using
    // Some sim systems can have 0 boundary entities in them. In this case, we have to ensure jitification does not fail
    unsigned int nAnalGMSafe = (nAnalGM > 0) ? nAnalGM : 1;
    strMap["_nAnalGMSafe_"] = std::to_string(nAnalGMSafe);
    strMap["_nActiveLoadingThreads_"] = std::to_string(NUM_ACTIVE_TEMPLATE_LOADING_THREADS);
    // nTotalBodyTopologies includes clump topologies and ext obj topologies
    strMap["_nDistinctMassProperties_"] = std::to_string(nDistinctMassProperties);
    strMap["_nJitifiableClumpComponents_"] = std::to_string(nJitifiableClumpComponents);
    strMap["_nMatTuples_"] = std::to_string(nMatTuples);
}

}  // namespace sgps