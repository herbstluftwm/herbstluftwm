## The 'herbstluftwm' executable ##

add_executable(herbstluftwm main.cpp)
install(TARGETS herbstluftwm DESTINATION ${BINDIR})

# additional sources – core/architectural stuff
target_sources(herbstluftwm PRIVATE
    arglist.cpp arglist.h
    attribute.cpp attribute.h attribute_.h
    byname.cpp byname.h
    childbyindex.cpp childbyindex.h
    child.h
    completion.h
    entity.cpp entity.h
    glib-backports.h
    globals.h
    hook.cpp hook.h
    object.cpp object.h
    optional.h
    root.cpp root.h
    signal.h
    link.h
    types.cpp types.h
    utils.cpp utils.h
    x11-types.cpp x11-types.h
    x11-utils.cpp x11-utils.h
    )

# additional sources – individual parts/modules
target_sources(herbstluftwm PRIVATE
    client.cpp client.h
    clientmanager.cpp clientmanager.h
    command.cpp command.h
    completion.h completion.cpp
    decoration.cpp decoration.h
    ewmh.cpp ewmh.h
    floating.cpp floating.h
    framedecoration.cpp framedecoration.h
    hookmanager.cpp hookmanager.h
    ipc-protocol.h
    ipc-server.cpp ipc-server.h
    key.cpp key.h
    keycombo.cpp keycombo.h
    keymanager.cpp keymanager.h
    layout.cpp layout.h
    monitor.cpp monitor.h
    monitormanager.cpp monitormanager.h
    mouse.cpp mouse.h
    namedhook.cpp namedhook.h
    rectangle.cpp rectangle.h
    rootcommands.cpp rootcommands.h
    rulemanager.cpp rulemanager.h
    rules.cpp rules.h
    settings.cpp settings.h
    stack.cpp stack.h
    tag.cpp tag.h
    tagmanager.cpp tagmanager.h
    tilingresult.cpp tilingresult.h
    tmp.cpp tmp.h
    xconnection.cpp xconnection.h
    xkeygrabber.cpp xkeygrabber.h
    )

# we require C++11
set_target_properties(herbstluftwm PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON)

## options
include(CMakeDependentOption)
cmake_dependent_option(WITH_XINERAMA "Use multi-monitor support" ON
    "X11_Xinerama_FOUND" OFF)

if (WITH_XINERAMA)
    set_property(SOURCE monitor.cpp PROPERTY COMPILE_DEFINITIONS XINERAMA)
    target_link_libraries(herbstluftwm PRIVATE ${X11_Xinerama_LIB})
endif()

## dependencies X11 & GLIB2 (link to Xext for XShape())
target_include_directories(herbstluftwm SYSTEM PUBLIC
    ${X11_X11_INCLUDE_PATH} ${X11_Xinerama_INCLUDE_PATH} ${GLIB2_INCLUDE_DIRS})
target_link_libraries(herbstluftwm PUBLIC
    ${X11_X11_LIB} ${X11_Xext_LIB} ${GLIB2_LIBRARIES})

## export variables to the code
# version string
export_version(main.cpp)
# systemwide autostart location
set_property(SOURCE main.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    HERBSTLUFT_GLOBAL_AUTOSTART=\"${CONFIGDIR}/autostart\")

# vim: et:ts=4:sw=4
