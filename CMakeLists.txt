# set range of known cmake versions where all policies are fine for us
cmake_minimum_required(VERSION 3.1...3.13)
if(${CMAKE_VERSION} VERSION_LESS 3.12) # do the best we can in this case
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

file(STRINGS VERSION VERSION) # read hlwm version from file 'VERSION'
project(Herbstluftwm
	VERSION ${VERSION}
	DESCRIPTION "A manual tiling window manager for X11")

# write all executables to root of build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

## macros & functions (may be moved to cmake/ and included)
# get project dependencies
macro (get_external_deps) # – macro for scope
	find_package(X11 REQUIRED)
	include(FindPkgConfig)
	pkg_check_modules(GLIB2 REQUIRED glib-2.0)
endmacro()

# extract git commit hash and append to project version
function (set_gitversion)
	set(PROJECT_VERSION_GIT ${PROJECT_VERSION} PARENT_SCOPE)
	if (EXISTS ${CMAKE_SOURCE_DIR}/.git)
		find_package(Git)
		if (GIT_FOUND)
			execute_process(
				COMMAND git rev-parse --short HEAD
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
				OUTPUT_VARIABLE GIT_HASH
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			string(TIMESTAMP DATE "%Y-%m-%d")
			set(PROJECT_VERSION_GIT 
				"${PROJECT_VERSION} (${GIT_HASH} built on ${DATE})"
				PARENT_SCOPE)
		endif()
	endif()
endfunction()

# export version string as a define
function (export_version SOURCEFILE)
	set_property(SOURCE ${SOURCEFILE} APPEND PROPERTY COMPILE_DEFINITIONS
		HERBSTLUFT_VERSION=\"${PROJECT_VERSION_GIT}\")
endfunction()

# add custom build flags to the DEBUG build type
function (add_debugflags)
	set(FLAGS
		-g -Og
		-pedantic -Wall -Werror
		-Wnull-dereference -Wdouble-promotion -Wformat=2
		-Wno-sign-compare -Wno-narrowing -Wno-error=cast-function-type
		-fsanitize=address,leak,undefined
		-fno-omit-frame-pointer)
	string(REPLACE ";" " " FLAGS "${FLAGS}")
	
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
			set(${VAR} ${FLAGS} CACHE STRING
			"Flags used during DEBUG builds." FORCE)
		endif()
	endforeach()
endfunction()

## install variables
# prefix for systemwide configuration, accompanies CMAKE_INSTALL_PREFIX
set(CMAKE_INSTALL_SYSCONF_PREFIX "/etc" CACHE PATH
	"Top-level install path for configuration files. May be given relative to CMAKE_INSTALL_PREFIX.")

set(CONFIGDIR "${CMAKE_INSTALL_SYSCONF_PREFIX}/xdg/herbstluftwm")
set(BINDIR bin)
set(DATADIR share)
set(MANDIR ${DATADIR}/man)
set(DOCDIR ${DATADIR}/doc/herbstluftwm)
set(LICENSEDIR ${DOCDIR}
	CACHE PATH "Install path for license file")
set(XSESSIONSDIR ${DATADIR}/xsessions
	CACHE PATH "Install path for xsession file")
set(ZSHCOMPLETIONDIR ${DATADIR}/zsh/functions/Completion/X
	CACHE PATH "Install path for zsh completions file")
set(BASHCOMPLETIONDIR ${CMAKE_INSTALL_SYSCONF_PREFIX}/bash_completion.d
	CACHE PATH "Install path for bash completions file")

## do the actual work
get_external_deps()
set_gitversion()
add_debugflags()
add_subdirectory(ipc-client)
add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(share)

# hack to set default build type to RELEASE
# from cmake devs, https://blog.kitware.com/cmake-and-the-default-build-type/
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

## install everything that was not installed from subdirectories
install(FILES BUGS NEWS INSTALL DESTINATION ${DOCDIR})
install(FILES LICENSE DESTINATION ${LICENSEDIR})
install(DIRECTORY scripts/ DESTINATION ${DOCDIR}/examples USE_SOURCE_PERMISSIONS)
