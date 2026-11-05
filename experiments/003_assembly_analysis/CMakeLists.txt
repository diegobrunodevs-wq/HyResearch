add_executable(HyruleResearch003
    main.cpp
    calculate.cpp
)

target_compile_features(HyruleResearch003 PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HyruleResearch003 PRIVATE /W4 /permissive-)
    set_source_files_properties(calculate.cpp PROPERTIES COMPILE_OPTIONS /FA)
endif()
