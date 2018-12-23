set(PROJECT_VERSION_GIT ${PROJECT_VERSION})
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
			"${PROJECT_VERSION} (${GIT_HASH} built on ${DATE})")
	endif()
endif()

# export version string as a define
function (export_version SOURCEFILE)
	set_property(SOURCE ${SOURCEFILE} APPEND PROPERTY COMPILE_DEFINITIONS
		HERBSTLUFT_VERSION=\"${PROJECT_VERSION_GIT}\")
endfunction()
