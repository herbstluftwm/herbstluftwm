add_custom_target("copy_autostart" ALL)
add_custom_command(
    TARGET copy_autostart POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/autostart
    ${CMAKE_CURRENT_BINARY_DIR}/autostart)

install(FILES herbstclient-completion.bash
    RENAME herbstclient
    DESTINATION ${BASHCOMPLETIONDIR})
install(FILES _herbstclient           DESTINATION ${ZSHCOMPLETIONDIR})
install(FILES herbstclient.fish       DESTINATION ${FISHCOMPLETIONDIR})
install(FILES herbstluftwm.desktop    DESTINATION ${XSESSIONSDIR})

install(PROGRAMS
    autostart
    panel.sh
    restartpanels.sh
    dmenu_run_hlwm
    DESTINATION ${CONFIGDIR}
)

# vim: et:ts=4:sw=4
