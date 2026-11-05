cmake_minimum_required(VERSION 3.25)

project(HyruleResearch001 LANGUAGES CXX)

add_executable(HyruleResearch001
    main.cpp
)

target_compile_features(HyruleResearch001 PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HyruleResearch001 PRIVATE
        /W4
        /permissive-
    )
endif()
