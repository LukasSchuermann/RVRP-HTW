cmake_minimum_required(VERSION 3.3)
set(CMAKE_CXX_STANDARD 17)

project(vrp)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif ()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fopenmp -DNDEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")


### This Project #####
add_subdirectory(src)

set(SOURCES
        src/arcflow_branching.c
        src/cmain.c
        src/cons_arcflow.c
        src/event_solution_vrp.c
        src/initial_vrp.c
        src/label_vrp.c
        src/labeling_algorithm_vrp.c
        src/labellist_vrp.c
        src/postprocessing_vrp.c
        src/pricer_vrp.c
        src/pricing_heuristic_vrp.c
        src/primal_heuristic_vrp.c
        src/probdata_vrp.c
        src/tools_data.c
        src/tools_vrp.c
        src/tools_evaluating.c
        src/vardata_vrp.c
        src/vehicleass_branching.c
        src/cons_vehicleass.c
)


add_executable(vrp ${SOURCES})

target_include_directories(vrp
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        )


# Note: SCIP_DIR as a path is only a recommendation for cmake
message(STATUS "Looking for SCIP.")
if (NOT SCIP_DIR)
    message(STATUS "No SCIP specified, will look for environment variable.")
    message(STATUS "VAR: $ENV{SCIP_DIR}")
    set(SCIP_DIR "$ENV{SCIP_DIR}" CACHE PATH "Path to SCIP directory")
else ()
    message(STATUS "Will use SCIP_DIR ${SCIP_DIR}.")
endif ()
find_package(SCIP PATHS ${SCIP_DIR} NO_DEFAULT_PATH REQUIRED)
message(STATUS "Using SCIP library from here ${SCIP_DIR}.")

## This project links
target_link_libraries(vrp
        ${SCIP_LIBRARIES}
        -lm
        )



