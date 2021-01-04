## The 'herbstclient' executable ##

add_executable(herbstclient main.c)
install(TARGETS herbstclient DESTINATION ${BINDIR})

# additional sources
target_sources(herbstclient PRIVATE
    client-utils.c	client-utils.h
    ipc-client.c	ipc-client.h
    )

# we require C99, X/Open 6 for POSIX 2004
set_target_properties(herbstclient PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    COMPILE_DEFINITIONS _XOPEN_SOURCE=600)

# dependencies X11
target_include_directories(herbstclient SYSTEM PUBLIC
    ${X11_X11_INCLUDE_PATH})
target_link_libraries(herbstclient PUBLIC
    ${X11_X11_LIB})

# communicate version string
export_version(main.c)

# vim: et:ts=4:sw=4
