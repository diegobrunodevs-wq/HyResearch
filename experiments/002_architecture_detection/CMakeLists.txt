add_executable(HyruleResearch002
    main.cpp
)

target_compile_features(HyruleResearch002 PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HyruleResearch002 PRIVATE /W4 /permissive-)
endif()
