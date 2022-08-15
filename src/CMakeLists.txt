## The 'herbstluftwm' executable ##

add_executable(herbstluftwm main.cpp)
install(TARGETS herbstluftwm DESTINATION ${BINDIR})

# additional sources – core/architectural stuff
target_sources(herbstluftwm PRIVATE
    arglist.cpp arglist.h
    argparse.cpp argparse.h
    attribute.cpp attribute.h attribute_.h
    autostart.cpp autostart.h
    boxstyle.cpp boxstyle.h
    byname.cpp byname.h
    child.h
    client.cpp client.h
    clientmanager.cpp clientmanager.h
    css.h css.cpp
    cssname.h cssname.cpp
    command.cpp command.h
    commandio.cpp commandio.h
    completion.h
    completion.h completion.cpp
    decoration.cpp decoration.h
    desktopwindow.h desktopwindow.cpp
    either.h
    entity.cpp entity.h
    ewmh.cpp ewmh.h
    finite.h
    fixprecdec.cpp fixprecdec.h
    floating.cpp floating.h
    font.cpp font.h
    fontdata.cpp fontdata.h
    framedata.h framedata.cpp
    framedecoration.cpp framedecoration.h
    frameparser.h frameparser.cpp
    frametree.h frametree.cpp
    globals.h
    globalcommands.cpp globalcommands.h
    hlwmcommon.cpp hlwmcommon.h
    hook.cpp hook.h
    indexingobject.h
    ipc-protocol.h
    ipc-server.cpp ipc-server.h
    keycombo.cpp keycombo.h
    keymanager.cpp keymanager.h
    layout.cpp layout.h
    link.h
    metacommands.cpp metacommands.h
    monitor.cpp monitor.h
    monitordetection.cpp monitordetection.h
    monitormanager.cpp monitormanager.h
    mouse.cpp mouse.h
    mousemanager.cpp mousemanager.h
    mousedraghandler.cpp mousedraghandler.h
    namedhook.cpp namedhook.h
    object.cpp object.h
    optional.h
    plainstack.h
    panelmanager.h panelmanager.cpp
    parserutils.h
    rectangle.cpp rectangle.h
    regexstr.cpp regexstr.h
    root.cpp root.h
    rulemanager.cpp rulemanager.h
    rules.cpp rules.h
    runtimeconverter.h
    settings.cpp settings.h
    signal.h
    stack.cpp stack.h
    tag.cpp tag.h
    tagmanager.cpp tagmanager.h
    theme.cpp theme.h
    tilingresult.cpp tilingresult.h
    tmp.cpp tmp.h
    converter.cpp converter.h
    typesdoc.cpp typesdoc.h
    utils.cpp utils.h
    watchers.h watchers.cpp
    widget.h widget.cpp
    x11-types.cpp x11-types.h
    x11-utils.cpp x11-utils.h
    x11-widgetrender.cpp x11-widgetrender.h
    xconnection.cpp xconnection.h
    xkeygrabber.cpp xkeygrabber.h
    xmainloop.cpp xmainloop.h
    unit.h
    )

# we require C++11
set_target_properties(herbstluftwm PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON)

## options
include(CMakeDependentOption)
cmake_dependent_option(WITH_XINERAMA "Use multi-monitor support via xinerama" ON
    "XINERAMA_FOUND" OFF)

if (WITH_XINERAMA)
    set_property(SOURCE monitordetection.cpp APPEND PROPERTY COMPILE_DEFINITIONS XINERAMA)
    target_link_libraries(herbstluftwm PRIVATE ${XINERAMA_LIBRARIES})
endif()

## dependencies X11 (link to Xext for XShape())
target_include_directories(herbstluftwm SYSTEM PUBLIC
    ${FREETYPE_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XFT_INCLUDE_DIRS}
    ${XEXT_INCLUDE_DIRS}
    ${XINERAMA_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
    ${XRENDER_INCLUDE_DIRS}
    ${XFIXES_INCLUDE_DIRS}
    )
target_link_libraries(herbstluftwm PUBLIC
    ${FREETYPE_LIBRARIES}
    ${X11_LIBRARIES}
    ${XEXT_LIBRARIES}
    ${XFT_LIBRARIES}
    ${XINERAMA_LIBRARIES}
    ${XRANDR_LIBRARIES}
    ${XRENDER_LIBRARIES}
    ${XFIXES_LIBRARIES}
    )

## export variables to the code
# version string
export_version(main.cpp)
# systemwide autostart location
set_property(SOURCE main.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    HERBSTLUFT_GLOBAL_AUTOSTART=\"${CONFIGDIR}/autostart\")

# vim: et:ts=4:sw=4
