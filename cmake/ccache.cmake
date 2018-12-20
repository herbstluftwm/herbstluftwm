find_program(CCACHE ccache)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_CCACHE "Use ccache for compilation" OFF "CCACHE" OFF)

if (ENABLE_CCACHE)
    message(STATUS "Using compiler wrapper: ${CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif()
