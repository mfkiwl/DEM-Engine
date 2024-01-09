//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//
//	SPDX-License-Identifier: BSD-3-Clause

// =============================================================================
// A meshed ball hitting a granular bed under gravity. A collection of different
// ball densities and drop heights are tested against loosely packed material.
// =============================================================================

#include <core/ApiVersion.h>
#include <core/utils/ThreadManager.h>
#include <DEM/API.h>
#include <DEM/HostSideHelpers.hpp>
#include <DEM/utils/Samplers.hpp>

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <random>

using namespace deme;
using namespace std::filesystem;

int main() {
    double terrain_rad = 0.01 / 2.;

    DEMSolver DEMSim;
    // Output less info at initialization
    DEMSim.SetVerbosity("ERROR");
    DEMSim.SetOutputFormat("CSV");
    DEMSim.SetOutputContent({"ABSV"});
    DEMSim.SetMeshOutputFormat("VTK");

    path out_dir = current_path();
    out_dir += "/DemoOutput_Force2D";
    create_directory(out_dir);

    // E, nu, CoR, mu, Crr...
    auto mat_type_terrain = DEMSim.LoadMaterial({{"E", 7e9}, {"nu", 0.24}, {"CoR", 0.9}, {"mu", 0.3}, {"Crr", 0.0}});

    float step_size = 2e-6;
    double world_size = 61 * terrain_rad;
    DEMSim.InstructBoxDomainDimension({-world_size / 2., world_size / 2.}, {-terrain_rad, terrain_rad},
                                      {0, 1 * world_size});
    DEMSim.InstructBoxDomainBoundingBC("top_open", mat_type_terrain);

    // Force model to use
    auto model2D = DEMSim.ReadContactForceModel("ForceModel2D.cu");
    model2D->SetMustHaveMatProp({"E", "nu", "CoR", "mu", "Crr"});
    model2D->SetMustPairwiseMatProp({"CoR", "mu", "Crr"});
    model2D->SetPerContactWildcards({"delta_time", "delta_tan_x", "delta_tan_y", "delta_tan_z"});

    std::vector<std::shared_ptr<DEMClumpTemplate>> templates_terrain;

    templates_terrain.push_back(DEMSim.LoadSphereType(terrain_rad * terrain_rad * terrain_rad * 1.0e3 * 4 / 3 * PI,
                                                      terrain_rad, mat_type_terrain));

    unsigned int num_particle = 0;
    float sample_z = 1.5 * terrain_rad;
    float fullheight = world_size * 0.20;
    float sample_halfwidth = world_size / 2 - 2 * terrain_rad;
    float init_v = 0.01;

    HCPSampler sampler(2.01 * terrain_rad);  // to be tested
    // PDSampler sampler(2.01 * terrain_rad);

    float3 sample_center = make_float3(0, 0, fullheight / 2 + 1 * terrain_rad);
    auto input_xyz = sampler.SampleBox(sample_center, make_float3(sample_halfwidth, 0.f, fullheight / 2.));
    std::vector<std::shared_ptr<DEMClumpTemplate>> template_to_use(input_xyz.size());
    for (unsigned int i = 0; i < input_xyz.size(); i++) {
        template_to_use[i] = templates_terrain[dist(gen)];
    }
    DEMSim.AddClumps(template_to_use, input_xyz);
    num_particle += input_xyz.size();

    std::cout << "Total num of particles: " << num_particle << std::endl;

    auto max_z_finder = DEMSim.CreateInspector("clump_max_z");
    auto total_mass_finder = DEMSim.CreateInspector("clump_mass");

    DEMSim.SetInitTimeStep(step_size);
    DEMSim.SetMaxVelocity(30.);
    DEMSim.SetGravitationalAcceleration(make_float3(0, 0, -9.81));

    DEMSim.Initialize();

    float sim_time = 3.0;
    float settle_time = 1.0;
    unsigned int fps = 20;
    float frame_time = 1.0 / fps;
    unsigned int out_steps = (unsigned int)(1.0 / (fps * step_size));

    std::cout << "Output at " << fps << " FPS" << std::endl;
    unsigned int currframe = 0;
    double terrain_max_z;

    // We can let it settle first
    for (float t = 0; t < settle_time; t += frame_time) {
        std::cout << "Frame: " << currframe << std::endl;
        char filename[200], meshfilename[200];
        sprintf(filename, "%s/DEMdemo_output_%04d.csv", out_dir.c_str(), currframe);
        sprintf(meshfilename, "%s/DEMdemo_mesh_%04d.vtk", out_dir.c_str(), currframe);
        DEMSim.WriteSphereFile(std::string(filename));
        DEMSim.WriteMeshFile(std::string(meshfilename));
        currframe++;

        DEMSim.DoDynamicsThenSync(frame_time);
        DEMSim.ShowThreadCollaborationStats();
    }

    char cp_filename[200];
    sprintf(cp_filename, "%s/bed.csv", out_dir.c_str());
    DEMSim.WriteClumpFile(std::string(cp_filename));

    // This is to show that you can change the material for all the particles in a family... although here,
    // mat_type_terrain_sim and mat_type_terrain are the same material so there is no effect; you can define
    // them differently though.
    DEMSim.SetFamilyClumpMaterial(0, mat_type_terrain_sim);
    DEMSim.DoDynamicsThenSync(0.2);
    terrain_max_z = max_z_finder->GetValue();
    float matter_mass = total_mass_finder->GetValue();
    float total_volume = (world_size * world_size) * (terrain_max_z - 0.);
    float bulk_density = matter_mass / total_volume;
    std::cout << "Original terrain height: " << terrain_max_z << std::endl;
    std::cout << "Bulk density: " << bulk_density << std::endl;

    // Then drop the ball
    DEMSim.ChangeFamily(2, 0);
    proj_tracker->SetPos(make_float3(0, 0, terrain_max_z + R + H));

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    for (float t = 0; t < sim_time; t += frame_time) {
        std::cout << "Frame: " << currframe << std::endl;
        char filename[200], meshfilename[200], cnt_filename[200];
        sprintf(filename, "%s/DEMdemo_output_%04d.csv", out_dir.c_str(), currframe);
        sprintf(meshfilename, "%s/DEMdemo_mesh_%04d.vtk", out_dir.c_str(), currframe);
        // sprintf(cnt_filename, "%s/Contact_pairs_%04d.csv", out_dir.c_str(), currframe);
        DEMSim.WriteSphereFile(std::string(filename));
        DEMSim.WriteMeshFile(std::string(meshfilename));
        // DEMSim.WriteContactFile(std::string(cnt_filename));
        currframe++;

        DEMSim.DoDynamics(frame_time);
        DEMSim.ShowThreadCollaborationStats();

        if (std::abs(proj_tracker->Vel().z) < 1e-4) {
            break;
        }
    }
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time_sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    std::cout << time_sec.count() << " seconds (wall time) to finish the simulation" << std::endl;

    DEMSim.ShowTimingStats();

    float3 final_pos = proj_tracker->Pos();
    std::cout << "Ball density: " << ball_density << std::endl;
    std::cout << "Ball rad: " << R << std::endl;
    std::cout << "Drop height: " << H << std::endl;
    std::cout << "Penetration: " << terrain_max_z - (final_pos.z - R) << std::endl;

    std::cout << "==============================================================" << std::endl;

    std::cout << "DEMdemo_BallDrop exiting..." << std::endl;
    return 0;
}
