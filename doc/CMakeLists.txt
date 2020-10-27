option(WITH_DOCUMENTATION "Build with documentation" ON)

function(gen_json_doc destfile)
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${destfile}.json")
    AUX_SOURCE_DIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/../src" CPPSRC)
    add_custom_target("doc_json" ALL DEPENDS ${dst})
    add_custom_command(
        OUTPUT ${dst}
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/gendoc.py" --sourcedir "${CMAKE_CURRENT_SOURCE_DIR}/../src" --json > ${dst}
        DEPENDS ${CPPSRC} "${CMAKE_CURRENT_SOURCE_DIR}/gendoc.py"
        )
    install(FILES ${dst} DESTINATION ${DOCDIR})
endfunction()

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
        COMMAND ${ASCIIDOC_A2X}
                --no-xmllint
                -f manpage
                -a \"herbstluftwmversion=herbstluftwm ${VERSION}\"
                -a \"date=`date +%Y-%m-%d`\"
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

gen_json_doc(hlwm-doc)

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
