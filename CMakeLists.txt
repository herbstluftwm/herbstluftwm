
cmake_minimum_required(VERSION 3.1)

set(CMAKE_BUILD_TYPE_INIT "Release")

cmake_policy(SET CMP0003 NEW)  # So library linking is more sane
cmake_policy(SET CMP0005 NEW)  # So strings are automatically quoted
cmake_policy(SET CMP0010 NEW)  # So syntax problems are errors
cmake_policy(SET CMP0014 NEW)  # Input directories must have CMakeLists.txt

set(CMAKE_C_STANDARD   99)
set(CMAKE_CXX_STANDARD 11)

# redirect output
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "" FORCE)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "" FORCE)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds aren't supported. Remove the CMakeCache.txt and run from another directory.")
endif()

project(Herbstluftwm)


# ----------------------------------------------------------------------------
# Options

option(WITH_DOCS "Build with documentation" ON)
option(WITH_XINERAMA "Use multi-monitor support" ON)

set(DESTDIR "" CACHE PATH "Root directory, prefix for CMAKE_INSTALL_PREFIX and CMAKE_INSTALL_SYSCONF_PREFIX when set")
set(CMAKE_INSTALL_SYSCONF_PREFIX "/etc" CACHE PATH "Directory to install configuration files")

set(SYSCONFDIR "${DESTDIR}/etc")
set(CONFIGDIR "${SYSCONFDIR}/xdg/herbstluftwm")

set(CMAKE_C_FLAGS "-pedantic -Wall")
set(CMAKE_CXX_FLAGS "-pedantic -Wall -Wno-sign-compare -Wno-narrowing -Wno-deprecated-register")


# ----------------------------------------------------------------------------
# Find Libraries

include(FindPkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)

if(WITH_XINERAMA)
    find_package(X11 REQUIRED)

    if(NOT X11_Xinerama_FOUND)
        set(WITH_XINERAMA OFF)
    endif()
endif()


# ----------------------------------------------------------------------------
# Find Vars

# VERSION_GIT
set(VERSION_GIT " (unknown)")
if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION_GIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(VERSION_GIT " (${VERSION_GIT})")
    endif()
endif()

# VERSION_* from 'version.mk'
file(STRINGS "${CMAKE_SOURCE_DIR}/version.mk" _contents REGEX "^VERSION_.*$")
string(REGEX REPLACE ".*VERSION_MAJOR[ \t]*=[ \t]*([0-9]+).*" "\\1" VERSION_MAJOR "${_contents}")
string(REGEX REPLACE ".*VERSION_MINOR[ \t]*=[ \t]*([0-9]+).*" "\\1" VERSION_MINOR "${_contents}")
string(REGEX REPLACE ".*VERSION_PATCH[ \t]*=[ \t]*([0-9]+).*" "\\1" VERSION_PATCH "${_contents}")
string(REGEX REPLACE ".*VERSION_SUFFIX[ \t]*=[ \t]*\"(.*)\".*" "\\1" VERSION_SUFFIX "${_contents}")
set(SHORTVERSION "${VERSION_MAJOR}\.${VERSION_MINOR}\.${VERSION_PATCH}${VERSION_SUFFIX}")
set(VERSION "${SHORTVERSION}${VERSION_GIT}")
unset(_contents)


# ----------------------------------------------------------------------------
# Program: 'herbstluftwm'

set(SRC
    src/clientlist.cpp
    src/command.cpp
    src/decoration.cpp
    src/desktopwindow.cpp
    src/ewmh.cpp
    src/floating.cpp
    src/hook.cpp
    src/ipc-server.cpp
    src/key.cpp
    src/layout.cpp
    src/main.cpp
    src/monitor.cpp
    src/mouse.cpp
    src/object.cpp
    src/rules.cpp
    src/settings.cpp
    src/stack.cpp
    src/tag.cpp
    src/utils.cpp
    src/x11-utils.cpp
)

set(INC_SYS
    ${X11_X11_INCLUDE_PATH}
    ${GLIB2_INCLUDE_DIRS}
)

set(DEF
    -D_XOPEN_SOURCE=600
    -DHERBSTLUFT_VERSION=\"${VERSION}\"
    -DHERBSTLUFT_VERSION_MAJOR=\"${VERSION_MAJOR}\"
    -DHERBSTLUFT_VERSION_MINOR=\"${VERSION_MINOR}\"
    -DHERBSTLUFT_VERSION_PATCH=\"${VERSION_PATCH}\"
    -DHERBSTLUFT_GLOBAL_AUTOSTART="${CONFIGDIR}/autostart"
)

set(LIB
    ${X11_X11_LIB}
    # for Xshape
    ${X11_Xext_LIB}
    ${GLIB2_LIBRARIES}
)

if(WITH_XINERAMA)
    list(APPEND INC_SYS ${X11_Xinerama_INCLUDE_PATH})
    list(APPEND DEF -DXINERAMA)
    list(APPEND LIB ${X11_Xinerama_LIB})
endif()

add_executable(herbstluftwm ${SRC})

target_include_directories(herbstluftwm SYSTEM PUBLIC ${INC_SYS})
target_compile_definitions(herbstluftwm PUBLIC ${DEF})
target_link_libraries(herbstluftwm ${LIB})


# ----------------------------------------------------------------------------
# Program: 'herbstclient'

set(INC_SYS
    ${GLIB2_INCLUDE_DIRS}
)

set(DEF
    -D_XOPEN_SOURCE=600
    -DHERBSTLUFT_VERSION=\"${VERSION}\"
    -DHERBSTLUFT_VERSION_MAJOR=\"${VERSION_MAJOR}\"
    -DHERBSTLUFT_VERSION_MINOR=\"${VERSION_MINOR}\"
    -DHERBSTLUFT_VERSION_PATCH=\"${VERSION_PATCH}\"
)

set(LIB
    ${X11_X11_LIB}
    ${GLIB2_LIBRARIES}
)

set(SRC
    ipc-client/client-utils.c
    ipc-client/ipc-client.c
    ipc-client/main.c
)

add_executable(herbstclient ${SRC})

target_include_directories(herbstclient SYSTEM PUBLIC ${INC_SYS})
target_compile_definitions(herbstclient PUBLIC ${DEF})
target_link_libraries(herbstclient ${LIB})


# ----------------------------------------------------------------------------
# Install

set(BINDIR ${DESTDIR}${CMAKE_INSTALL_PREFIX}/bin)
set(DATADIR ${DESTDIR}${CMAKE_INSTALL_PREFIX}/share)
set(MANDIR ${DATADIR}/man)
set(DOCDIR ${DATADIR}/doc/herbstluftwm)
set(EXAMPLESDIR ${DOCDIR}/examples)
set(LICENSEDIR ${DOCDIR})
set(XSESSIONSDIR ${DATADIR}/xsessions)
set(ZSHCOMPLETIONDIR ${DATADIR}/zsh/functions/Completion/X)
set(BASHCOMPLETIONDIR ${SYSCONFDIR}/bash_completion.d)


install(
    PROGRAMS
        ${CMAKE_BINARY_DIR}/bin/herbstluftwm
        ${CMAKE_BINARY_DIR}/bin/herbstclient
        ${CMAKE_SOURCE_DIR}/share/dmenu_run_hlwm
    DESTINATION
        ${BINDIR}
)

install(
    FILES
        ${CMAKE_SOURCE_DIR}/BUGS
        ${CMAKE_SOURCE_DIR}/NEWS
        ${CMAKE_SOURCE_DIR}/INSTALL
    DESTINATION
        ${DOCDIR}
)

install(
    FILES
        ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION
        ${LICENSEDIR}
)

install(
    PROGRAMS
        ${CMAKE_SOURCE_DIR}/share/autostart
        ${CMAKE_SOURCE_DIR}/share/panel.sh
        ${CMAKE_SOURCE_DIR}/share/restartpanels.sh
    DESTINATION
        ${CONFIGDIR}
)

install(
    FILES
        ${CMAKE_SOURCE_DIR}/share/herbstclient-completion
    DESTINATION
        ${BASHCOMPLETIONDIR}
)
install(
    FILES
        ${CMAKE_SOURCE_DIR}/share/_herbstclient
    DESTINATION
        ${ZSHCOMPLETIONDIR}
)
install(
    FILES
        ${CMAKE_SOURCE_DIR}/share/herbstluftwm.desktop
    DESTINATION
        ${XSESSIONSDIR}
)


install(
    FILES
        ${CMAKE_SOURCE_DIR}/scripts/README
    DESTINATION
        ${EXAMPLESDIR}
)
install(
    PROGRAMS
        ${CMAKE_SOURCE_DIR}/scripts/README
        ${CMAKE_SOURCE_DIR}/scripts/dmenu.sh
        ${CMAKE_SOURCE_DIR}/scripts/dumpbeautify.sh
        ${CMAKE_SOURCE_DIR}/scripts/exec_on_tag.sh
        ${CMAKE_SOURCE_DIR}/scripts/execwith.sh
        ${CMAKE_SOURCE_DIR}/scripts/floatmon.sh
        ${CMAKE_SOURCE_DIR}/scripts/herbstcommander.sh
        ${CMAKE_SOURCE_DIR}/scripts/keychain.sh
        ${CMAKE_SOURCE_DIR}/scripts/lasttag.sh
        ${CMAKE_SOURCE_DIR}/scripts/layout.sh
        ${CMAKE_SOURCE_DIR}/scripts/loadstate.sh
        ${CMAKE_SOURCE_DIR}/scripts/q3terminal.sh
        ${CMAKE_SOURCE_DIR}/scripts/savestate.sh
        ${CMAKE_SOURCE_DIR}/scripts/scratchpad.sh
        ${CMAKE_SOURCE_DIR}/scripts/wselect.sh
    DESTINATION
        ${EXAMPLESDIR}
)

if(WITH_DOCS)
    function(doc_manpage_gen sourcefile man_nr)
        set(sourcefile_target "doc_man_${sourcefile}")

        set(src_orig "${CMAKE_SOURCE_DIR}/doc/${sourcefile}.txt")
        set(src "${CMAKE_BINARY_DIR}/doc/${sourcefile}.txt")
        set(dst "${CMAKE_BINARY_DIR}/doc/${sourcefile}.${man_nr}")

        add_custom_target(
            ${sourcefile_target}
            DEPENDS ${dst}
        )
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/doc/"
            COMMAND ${CMAKE_COMMAND} -E copy ${src_orig} ${src}
            COMMAND a2x -f manpage
                        -a \"herbstluftwmversion=herbstluftwm ${VERSION}\"
                        -a \"date=`date +%Y-%m-%d`\"
                        ${src}
            DEPENDS ${src_orig}
        )
        add_dependencies(herbstluftwm ${sourcefile_target})

        install(FILES ${dst} DESTINATION "${MANDIR}/man${man_nr}")
    endfunction()

    function(doc_html_gen sourcefile)
        set(sourcefile_target "doc_html_${sourcefile}")

        set(src "${CMAKE_SOURCE_DIR}/doc/${sourcefile}.txt")
        set(dst "${CMAKE_BINARY_DIR}/doc/${sourcefile}.html")

        add_custom_target(
            ${sourcefile_target}
            DEPENDS ${dst}
        )
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/doc/"
            COMMAND asciidoc -o ${dst} ${src}
            DEPENDS ${src}
        )

        add_dependencies(herbstluftwm ${sourcefile_target})

        install(FILES ${dst} DESTINATION ${DOCDIR})
    endfunction()

    doc_manpage_gen(herbstclient 1)
    doc_manpage_gen(herbstluftwm 1)
    doc_manpage_gen(herbstluftwm-tutorial 7)

    doc_html_gen(herbstclient)
    doc_html_gen(herbstluftwm)
    doc_html_gen(herbstluftwm-tutorial)
endif()

