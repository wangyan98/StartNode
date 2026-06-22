include(CheckCXXCompilerFlag)

add_library(MetaCore INTERFACE)
target_include_directories(MetaCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(MetaCore INTERFACE cxx_std_26)

check_cxx_compiler_flag(-freflection META_HAS_REFLECTION)
if(META_HAS_REFLECTION)
    target_compile_options(MetaCore INTERFACE -freflection)
else()
    message(WARNING "Compiler does not support -freflection. Reflection features will be disabled.")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    check_cxx_compiler_flag(-fexpansion-statements META_HAS_EXPANSION_STATEMENTS)
    if(META_HAS_EXPANSION_STATEMENTS)
        target_compile_options(MetaCore INTERFACE -fexpansion-statements)
    endif()

    check_cxx_compiler_flag(-fannotation-attributes META_HAS_ANNOTATION_ATTRIBUTES)
    if(META_HAS_ANNOTATION_ATTRIBUTES)
        target_compile_options(MetaCore INTERFACE -fannotation-attributes)
    endif()

    check_cxx_compiler_flag(-fparameter-reflection META_HAS_PARAMETER_REFLECTION)
    if(META_HAS_PARAMETER_REFLECTION)
        target_compile_options(MetaCore INTERFACE -fparameter-reflection)
    endif()
endif()
