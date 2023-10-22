//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//
//	SPDX-License-Identifier: BSD-3-Clause

// =============================================================================
// This benchmark test the angle of repose of a given material using a drum test.
//  Set by btagliafierro 28 Aug 2023
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

void runDEME(std::string dir_output, float friction, float rollingMaterial);

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cout << "You have entered " << argc << " arguments, which is wrong!" << std::endl;
        return 0;
    }

    int case_Folder = atoi(argv[1]);          // takes the test ID
    int case_ID = atoi(argv[2]);              // takes the test ID
    float conctact_friction = atof(argv[3]);  // takes the value
    float rolling_friction = atof(argv[4]);   // takes the value

    std::string out_dir = "/Test_FlowSphere/";
    out_dir += "Drum_" + std::to_string(case_Folder) + "/" + std::to_string(case_ID) + "/";

    std::cout << "Running case with friction: " << conctact_friction << ", and rolling friction: " << rolling_friction
              << std::endl;
    std::cout << "Dir out is " << out_dir << std::endl;

    runDEME(out_dir, conctact_friction, rolling_friction);

    return 0;
}

void runDEME(std::string dir_output, float frictionMaterial, float rollingMaterial) {
    DEMSolver DEMSim;
    DEMSim.UseFrictionalHertzianModel();
    DEMSim.SetVerbosity(INFO);
    DEMSim.SetOutputFormat(OUTPUT_FORMAT::CSV);
    DEMSim.SetOutputContent(OUTPUT_CONTENT::XYZ);
    DEMSim.EnsureKernelErrMsgLineNum();

    srand(7001);
    DEMSim.SetCollectAccRightAfterForceCalc(true);
    DEMSim.SetErrorOutAvgContacts(50);

    path out_dir = current_path();
    out_dir += dir_output;
    // Scale factor
    float scaling = 1.f;
    float diamDrum = 0.12;

    // total number of random clump templates to generate

    double radius = 0.003300 * scaling / 2.0;
    double density = 1410;

    int totalSpheres = 17000;

    int num_template = 1;

    float plane_bottom = -3 * diamDrum / 10 * scaling;

    std::vector<double> angular = {3.60};  // value given in rpm

    auto mat_type_walls = DEMSim.LoadMaterial({{"E", 2.0e9}, {"nu", 0.3}, {"CoR", 0.60}, {"mu", 0.04}, {"Crr", 0.04}});

    auto mat_type_particles =
        DEMSim.LoadMaterial({{"E", 2.7e9}, {"nu", 0.35}, {"CoR", 0.85}, {"mu", frictionMaterial}, {"Crr", 0.01}});

    DEMSim.SetMaterialPropertyPair("CoR", mat_type_walls, mat_type_particles, 0.5);
    DEMSim.SetMaterialPropertyPair("Crr", mat_type_walls, mat_type_particles, 0.02);
    DEMSim.SetMaterialPropertyPair("mu", mat_type_walls, mat_type_particles, 0.20);

    // Make ready for simulation
    float step_size = 2.50e-6;
    DEMSim.InstructBoxDomainDimension({-0.09, 0.09}, {-0.15, 0.15}, {-0.15, 0.15});
    DEMSim.InstructBoxDomainBoundingBC("top_open", mat_type_walls);
    DEMSim.SetInitTimeStep(step_size);
    DEMSim.SetGravitationalAcceleration(make_float3(0, 0, -9.81));
    // Max velocity info is generally just for the solver's reference and the user do not have to set it. The solver
    // wouldn't take into account a vel larger than this when doing async-ed contact detection: but this vel won't
    // happen anyway and if it does, something already went wrong.
    DEMSim.SetMaxVelocity(25.);
    DEMSim.SetInitBinSize(radius * 5);

    // Loaded meshes are by-default fixed
    auto fixed = DEMSim.AddWavefrontMeshObject("../data/granularFlow/drum.obj", mat_type_walls);

    fixed->Scale(diamDrum * 1.0);
    fixed->SetFamily(10);

    DEMSim.SetFamilyPrescribedAngVel(10, to_string_with_precision(-2.0 * PI * angular[0] / 60.0), "0.0", "0.0");
    DEMSim.SetFamilyPrescribedAngVel(11, to_string_with_precision(-2.0 * PI * angular[1] / 60.0), "0.0", "0.0");
    DEMSim.SetFamilyPrescribedAngVel(12, to_string_with_precision(-2.0 * PI * angular[2] / 60.0), "0.0", "0.0");
    DEMSim.SetFamilyPrescribedAngVel(13, to_string_with_precision(-2.0 * PI * angular[3] / 60.0), "0.0", "0.0");

    auto max_z_finder = DEMSim.CreateInspector("clump_max_z");
    auto min_z_finder = DEMSim.CreateInspector("clump_min_z");
    auto total_mass_finder = DEMSim.CreateInspector("clump_mass");
    auto max_v_finder = DEMSim.CreateInspector("clump_max_absv");

    // Make an array to store these generated clump templates
    std::vector<std::shared_ptr<DEMClumpTemplate>> clump_types;
    std::default_random_engine generator;
    std::normal_distribution<double> distribution(radius, radius * 0.05);
    double maxRadius = 0;

    for (int i = 0; i < num_template; i++) {
        std::vector<float> radii;
        std::vector<float3> relPos;
        std::vector<std::shared_ptr<DEMMaterial>> mat;

        //double radiusMax = distribution(generator);
        double radiusMax = radius;
        double radiusMin = 8.0 / 8.0 * radiusMax;
        double eccentricity = 0.0 / 8.0 * radiusMax;

        radii.push_back(radiusMin);
        float3 tmp;
        tmp.x = -0.0 * eccentricity / 2.0;
        tmp.y = 0;
        tmp.z = 0;
        relPos.push_back(tmp);
        mat.push_back(mat_type_particles);

        double c = radiusMin;  // smaller dim of the ellipse
        double b = radiusMin;
        double a = radiusMin + 0.50 * eccentricity;

        float mass = 4.0 / 3.0 * PI * a * b * c * density;
        float3 MOI = make_float3(1.f / 5.f * mass * (b * b + c * c), 1.f / 5.f * mass * (a * a + c * c),
                                 1.f / 5.f * mass * (b * b + a * a));
        std::cout << a << " chosen moi ..." << a / radius << std::endl;

        maxRadius = (radiusMax > maxRadius) ? radiusMax : maxRadius;
        auto clump_ptr = DEMSim.LoadClumpType(mass, MOI, radii, relPos, mat_type_particles);
        // clump_ptr->AssignName("fsfs");
        clump_types.push_back(clump_ptr);
    }
    std::cout << " maximum radius is " << maxRadius / radius << std::endl;

    unsigned int currframe = 0;
    unsigned int curr_step = 0;
    float settle_frame_time = 0.004;

    remove_all(out_dir);
    create_directories(out_dir);

    char filename[200], meshfile[200];

    float shift_xyz = 1.2 * (maxRadius) * 2.0;
    float x = 0;
    float y = 0;

    float z = shift_xyz / 2;  // by default we create beads at 0
    double emitterZ = 3 * diamDrum / 8;
    unsigned int actualTotalSpheres = 0;

    DEMSim.Initialize();

    int frame = 0;
    bool generate = true;
    bool initialization = true;
    double timeTotal = 0;
    double consolidation = true;

    sprintf(meshfile, "%s/DEMdemo_mesh_%04d.vtk", out_dir.c_str(), frame);
    DEMSim.WriteMeshFile(std::string(meshfile));

    while (initialization) {
        std::vector<std::shared_ptr<DEMClumpTemplate>> input_pile_template_type;
        std::vector<float3> input_pile_xyz;
        PDSampler sampler(shift_xyz);

        bool generate = (plane_bottom + shift_xyz / 2 > emitterZ) ? false : true;

        if (generate) {
            float sizeZ = (frame == 0) ? 3 * diamDrum / 5 : 0.00;
            float sizeX = diamDrum;
            float z = plane_bottom + sizeZ / 2;

            float3 center_xyz = make_float3(0, 0, 0);

            std::cout << "level of particles position ... " << center_xyz.z << std::endl;

            auto heap_particles_xyz =
                sampler.SampleCylinderX(center_xyz, diamDrum / 2 - shift_xyz / 2, diamDrum / 2 - shift_xyz / 2);
            unsigned int num_clumps = heap_particles_xyz.size();
            std::cout << "number of particles at this level ... " << num_clumps << std::endl;

            for (unsigned int i = actualTotalSpheres; i < actualTotalSpheres + num_clumps; i++) {
                input_pile_template_type.push_back(clump_types.at(i % num_template));
            }

            input_pile_xyz.insert(input_pile_xyz.end(), heap_particles_xyz.begin(), heap_particles_xyz.end());

            auto the_pile = DEMSim.AddClumps(input_pile_template_type, input_pile_xyz);
            the_pile->SetVel(make_float3(-0.00, 0.0, -0.50));
            the_pile->SetFamily(100);

            DEMSim.UpdateClumps();

            std::cout << "Total num of particles: " << (int)DEMSim.GetNumClumps() << std::endl;
            actualTotalSpheres = (int)DEMSim.GetNumClumps();
            // Generate initial clumps for piling
        }
        timeTotal += settle_frame_time;
        // std::cout << "Total runtime: " << timeTotal << "s; settling for: " << settle_frame_time << std::endl;
        // std::cout << "maxZ is: " << max_z_finder->GetValue() << std::endl;

        initialization = (actualTotalSpheres < totalSpheres) ? true : false;

        if (generate && !(frame % 1)) {
            std::cout << "frame : " << frame << std::endl;
            sprintf(filename, "%s/DEMdemo_settling.csv", out_dir.c_str());
            DEMSim.WriteSphereFile(std::string(filename));
            // sprintf(meshfile, "%s/DEMdemo_mesh.vtk", out_dir.c_str());
            // DEMSim.WriteMeshFile(std::string(meshfile));
            // DEMSim.ShowThreadCollaborationStats();
            frame++;
        }
        //frame++;

        DEMSim.DoDynamicsThenSync(settle_frame_time);

        plane_bottom = max_z_finder->GetValue();
    }

    std::cout << "Initialization done with : " << actualTotalSpheres << "particles" << std::endl;

    float timeStep = 5e-3;
    int numStep = 5.0f / timeStep;
    int numChangeSim = 5.0f / timeStep;
    int timeOut = int(0.05f / timeStep);

    // std::cout << "Time out in time steps is: " << timeOut << std::endl;
    frame = 0;

    int counterSim = 0;

    for (int i = 0; i < numStep; i++) {
        if (!(i % timeOut) || i == 0) {
            sprintf(filename, "%s/DEMdemo_output_%04d.csv", out_dir.c_str(), frame);
            sprintf(meshfile, "%s/DEMdemo_mesh_%04d.vtk", out_dir.c_str(), frame);

            DEMSim.WriteMeshFile(std::string(meshfile));
            DEMSim.WriteSphereFile(std::string(filename));

            // std::cout << "Frame: " << frame << std::endl;
            std::cout << "Elapsed time: " << timeStep * (i) << std::endl;
            // DEMSim.ShowThreadCollaborationStats();
            frame++;
        }

        if (!(i % numChangeSim) && i > 0) {
            DEMSim.DoDynamicsThenSync(0);
            std::cout << "change family of drum to " << 10 + 1 + counterSim << " " << std::endl;
            DEMSim.ChangeFamily(10 + counterSim, 10 + 1 + counterSim);
            counterSim++;
        }

        DEMSim.DoDynamics(timeStep);
    }

    DEMSim.ShowTimingStats();
    DEMSim.ShowAnomalies();
    DEMSim.ClearTimingStats();

    std::cout << "DEME exiting..." << std::endl;
}
