cmake_minimum_required(VERSION 3.1...3.13)

file(STRINGS VERSION VERSION) # read hlwm version from file 'VERSION'
project(Herbstluftwm
    VERSION ${VERSION})

# use our cmake dir for include(…) statements
list(INSERT CMAKE_MODULE_PATH 0 "${PROJECT_SOURCE_DIR}/cmake")

# make CMake behave our way
include(behavior)
# get project dependencies
include(dependencies)
# generate variable PROJECT_VERSION_GIT, provide function export_version()
include(gitversion)
# add custom (default) debug flags in variable cache
include(debugflags)
# find ccache (offered via ENABLE_CCACHE if available)
include(ccache)

## set install variables
# prefix for systemwide configuration, accompanies CMAKE_INSTALL_PREFIX
set(CMAKE_INSTALL_SYSCONF_PREFIX "/etc" CACHE PATH
    "Top-level install path for configuration files. May be given relative to CMAKE_INSTALL_PREFIX.")

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CONFIGDIR "${CMAKE_INSTALL_SYSCONF_PREFIX}/xdg/herbstluftwm")
set(BINDIR bin)
set(DATADIR share)
set(MANDIR ${DATADIR}/man)
set(DOCDIR ${DATADIR}/doc/herbstluftwm)
set(LICENSEDIR ${DOCDIR}
    CACHE PATH "Install path for license file")
set(XSESSIONSDIR ${DATADIR}/xsessions
    CACHE PATH "Install path for xsession file")
set(ZSHCOMPLETIONDIR ${DATADIR}/zsh/site-functions
    CACHE PATH "Install path for zsh completions file")
set(FISHCOMPLETIONDIR ${DATADIR}/fish/vendor_completions.d
    CACHE PATH "Install path for fish completions file")
set(BASHCOMPLETIONDIR ${DATADIR}/bash-completion/completions
    CACHE PATH "Install path for bash completions file")

## do the actual work
add_subdirectory(ipc-client)
add_subdirectory(src)
add_subdirectory(share)

## install everything that was not installed from subdirectories
install(FILES BUGS NEWS DESTINATION ${DOCDIR})
install(FILES LICENSE DESTINATION ${LICENSEDIR})
install(DIRECTORY scripts/ DESTINATION ${DOCDIR}/examples USE_SOURCE_PERMISSIONS)

# vim: et:ts=4:sw=4
