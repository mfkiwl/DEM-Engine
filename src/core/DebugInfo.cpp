//	Copyright (c) 2021, SBEL GPU Development Team
//	Copyright (c) 2021, University of Wisconsin - Madison
//	All rights reserved.

#include <iostream>

#include <core/ApiVersion.h>

namespace sgps {

void versionInfo() {
	// Project Info
	std::cout << "SBEL GPU Physics Solvers (c) 2021" << std::endl;
	std::cout << "Project Version: " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl;
	
	// C++ Info
	std::cout << "C++ Standard Revision: " << __cplusplus << std::endl;
}

} // namespace sgps