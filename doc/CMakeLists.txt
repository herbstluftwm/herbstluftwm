option(WITH_DOCUMENTATION "Build with documentation" ON)

function(gen_manpage sourcefile man_nr)
    set(src_orig "${CMAKE_CURRENT_SOURCE_DIR}/${sourcefile}.txt")
    set(src "${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.txt")
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.${man_nr}")

    add_custom_target("doc_man_${sourcefile}" ALL DEPENDS ${dst})
    add_custom_command(
        OUTPUT ${dst}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
        # asciidoc doesn't support destination directory for manpages
        COMMAND ${CMAKE_COMMAND} -E copy ${src_orig} ${src}
        STRING(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)
        COMMAND ${ASCIIDOC_A2X}
                -f manpage
                -a \"herbstluftwmversion=herbstluftwm ${VERSION}\"
                -a \"date=${BUILD_DATE}\"
                ${src}
        DEPENDS ${src_orig}
        )
    install(FILES ${dst} DESTINATION "${MANDIR}/man${man_nr}")
endfunction()

function(gen_html sourcefile)
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${sourcefile}.txt")
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.html")

    add_custom_target("doc_html_${sourcefile}" ALL DEPENDS ${dst})
    add_custom_command(
        OUTPUT ${dst}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${ASCIIDOC} -o ${dst} ${src}
        DEPENDS ${src}
        )
    install(FILES ${dst} DESTINATION ${DOCDIR})
endfunction()

if (WITH_DOCUMENTATION)
    find_program(ASCIIDOC_A2X NAMES a2x DOC "Path to AsciiDoc a2x command")
    find_program(ASCIIDOC NAMES asciidoc DOC "Path to AsciiDoc command")

    gen_manpage(herbstclient 1)
    gen_manpage(herbstluftwm 1)
    gen_manpage(herbstluftwm-tutorial 7)

    gen_html(herbstclient)
    gen_html(herbstluftwm)
    gen_html(herbstluftwm-tutorial)
endif()

# vim: et:ts=4:sw=4
