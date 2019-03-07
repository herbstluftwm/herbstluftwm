# add custom build flags to the DEBUG build type

set(FLAGS
    # General debug flags:
    -g -Og -fno-omit-frame-pointer --coverage
    -Werror

    # Sanitizers:
    -fsanitize=address,leak,undefined

    # All kinds of pedantic warnings:
    -pedantic -Wall -Wextra
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wshadow

    # Disable unused parameter warnings (most cases are on purpose):
    -Wno-unused-parameter

    # Disable warnings that trigger due to known unresolved issues:
    -Wno-sign-compare -Wno-narrowing
    )

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # Add GCC-specific flags
    list(APPEND FLAGS
        # Additional warnings that don't come with Wall/Wextra:
        -Wduplicated-cond -Wduplicated-branches -Wlogical-op

        # This is since in GCC-8 and triggers on known issues:
        -Wno-error=cast-function-type
        )

    # Add GCC- *and* C++-specific flags
    set(CXX_FLAGS ${CXX_FLAGS}
        # Additional warnings that don't come with Wall/Wextra:
        -Wuseless-cast
        )
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # Add clang-specific flags
    list(APPEND FLAGS
        # Additional warnings that don't come with Wall/Wextra:
        -Wshadow-all
        )
endif()

string(REPLACE ";" " " FLAGS "${FLAGS}")
string(REPLACE ";" " " CXX_FLAGS "${CXX_FLAGS}")

# Note: it is discouraged in CMake to alter these variables, but instead you
# should set flags on the target. However, those flags are hidden from the
# user. In our case, we want the user to be able to see and alter the
# flags of the build type.
set(TYPES C;CXX;EXE_LINKER)
foreach (TYPE ${TYPES})
    set(VAR "CMAKE_${TYPE}_FLAGS_DEBUG")

    # Note: we need to apply FORCE for our flags to override the default,
    # but we don't want to override user changes. So we only override if
    # empty or default ("-g")
    string(COMPARE EQUAL "${${VAR}}" "-g" ISDEFAULT)
    if (NOT ${VAR} OR "${ISDEFAULT}")
        set(${VAR} "${FLAGS} ${${TYPE}_FLAGS}" CACHE STRING
            "Flags used during DEBUG builds." FORCE)
    endif()
endforeach()

# vim: et:ts=4:sw=4
