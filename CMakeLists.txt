cmake_minimum_required(VERSION 3.0)

project(sddc_connector VERSION 0.1)
add_definitions(-DVERSION="${PROJECT_VERSION}-dev")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)

enable_language(CUDA)
set(CMAKE_CUDA_ARCHITECTURES 60)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified: defaulting to release.")
endif()

include(GNUInstallDirs)

find_package(OwrxConnector 0.7 REQUIRED)
find_package(Csdr 0.19 REQUIRED)

add_subdirectory(src)