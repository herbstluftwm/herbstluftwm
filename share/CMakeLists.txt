install(FILES herbstclient-completion DESTINATION ${BASHCOMPLETIONDIR})
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
