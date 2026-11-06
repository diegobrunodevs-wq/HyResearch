
add_executable(HyruleResearch004Tests
    parser_tests.cpp
    parser.cpp
)

target_compile_features(HyruleResearch004Tests PRIVATE cxx_std_20)

add_executable(HyruleResearch004TranslatorTests
    translator_tests.cpp
    parser.cpp
    translator.cpp
)

target_compile_features(HyruleResearch004TranslatorTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HyruleResearch004Tests PRIVATE /W4 /permissive-)
    target_compile_options(HyruleResearch004TranslatorTests PRIVATE /W4 /permissive-)
endif()


add_executable(HyruleResearch004SemanticTests
    semantic_tests.cpp
    semantic.cpp
    parser.cpp
    translator.cpp
)

target_compile_features(HyruleResearch004SemanticTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HyruleResearch004SemanticTests PRIVATE /W4 /permissive-)
endif()

add_executable(HyruleResearch004NativeCodeTests
    native_code_generator_tests.cpp
    native_code_generator.cpp
    parser.cpp
)

target_compile_features(HyruleResearch004NativeCodeTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HyruleResearch004NativeCodeTests PRIVATE /W4 /permissive-)
endif()