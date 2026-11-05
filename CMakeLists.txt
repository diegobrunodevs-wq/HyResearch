cmake_minimum_required(VERSION 3.25)

project(HyruleResearch
    VERSION 0.1.0
    DESCRIPTION "Native PC Port Feasibility Research"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HYRULE_BUILD_EXPERIMENTS "Build research experiments" ON)

if(HYRULE_BUILD_EXPERIMENTS)
    add_subdirectory(experiments/001_native_execution)
endif()
