option(WITH_DOCUMENTATION "Build with documentation" ON)

# don't rebuilt but instead only copy the documentation if hlwm-doc.json exists
# in the source directory
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hlwm-doc.json")
    set(doc_exists_in_src ON)
else()
    set(doc_exists_in_src OFF)
endif()
option(COPY_DOCUMENTATION "Use pre-built documentation (only available in tarballs)" ${doc_exists_in_src})

add_custom_target("all_doc")

function(gen_json_doc destfile)
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${destfile}.json")
    AUX_SOURCE_DIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/../src" CPPSRC)
    add_custom_target("doc_json" DEPENDS ${dst})
    if (COPY_DOCUMENTATION)
        set(dst_prebuilt "${CMAKE_CURRENT_SOURCE_DIR}/${destfile}.json")
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E copy ${dst_prebuilt} ${dst}
            DEPENDS ${dst_prebuilt}
            COMMENT "Using pre-built ${dst_prebuilt}"
            )
    else()
        add_custom_command(
            OUTPUT ${dst}
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/gendoc.py" --sourcedir "${CMAKE_CURRENT_SOURCE_DIR}/../src" --json > ${dst}
            DEPENDS ${CPPSRC} "${CMAKE_CURRENT_SOURCE_DIR}/gendoc.py"
            )
    endif()
    install(FILES ${dst} DESTINATION ${DOCDIR} OPTIONAL)
endfunction()

function(gen_object_asciidoc jsonfile destfile)
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${destfile}")
    set(absjson "${CMAKE_CURRENT_BINARY_DIR}/${jsonfile}")
    add_custom_target(gen_object_asciidoc DEPENDS ${dst} doc_json)
    if (COPY_DOCUMENTATION)
        set(dst_prebuilt "${CMAKE_CURRENT_SOURCE_DIR}/${destfile}")
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E copy ${dst_prebuilt} ${dst}
            DEPENDS ${dst_prebuilt}
            COMMENT "Using pre-built ${dst_prebuilt}"
            )
    else()
        add_custom_command(
            OUTPUT ${dst}
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/format-doc.py" ${absjson} > ${dst}
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/format-doc.py" ${absjson}
            )
    endif()
endfunction()

function(gen_manpage sourcefile man_nr)
    # additional arguments (${ARGN}) are passed as DEPENDS to the asciidoc command
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${sourcefile}.txt")
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.${man_nr}")
    set(docbookxml "${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.xml")
    STRING(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

    # as a hack, every man page depends on gen_object_asciidoc, even though only
    # 'herbstluftwm' needs it. We do this to avoid that hlwm-doc.json is built
    # multiple times when make is invoked with -j8
    add_custom_target("doc_man_${sourcefile}" ALL DEPENDS ${dst} gen_object_asciidoc)
    add_dependencies(all_doc "doc_man_${sourcefile}")
    if (COPY_DOCUMENTATION)
        set(dst_prebuilt "${CMAKE_CURRENT_SOURCE_DIR}/${sourcefile}.${man_nr}")
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E copy ${dst_prebuilt} ${dst}
            DEPENDS ${dst_prebuilt}
            COMMENT "Using pre-built ${dst_prebuilt}"
            )
    else()
        # if the documentation is built from scratch, add an intermediate step for
        # the xml-based docbook format.
        # add_dependencies("doc_man_${sourcefile}" "doc_man_${sourcefile}_xml")
        # add_custom_target("doc_man_${sourcefile}_xml" ALL DEPENDS ${docbookxml})
        add_custom_command(
            OUTPUT ${dst} ${docbookxml}
            # first create the docbook xml:
            COMMAND ${ASCIIDOC}
                    -o ${docbookxml}
                    -b docbook45
                    -a \"herbstluftwmversion=herbstluftwm ${VERSION}\"
                    -a \"date=${BUILD_DATE}\"
                    -a \"builddir=${CMAKE_CURRENT_BINARY_DIR}\"
                    ${src}
            # then, patch the xml:
            COMMAND ${XSLTPROC}
                --output ${docbookxml}
                ${CMAKE_CURRENT_SOURCE_DIR}/literal2emph.xsl
                ${docbookxml}
            # and finally, create the man page from it:
            COMMAND ${XSLTPROC}
                --output ${dst}
                --stringparam callout.graphics 0
                --stringparam navig.graphics 0
                --stringparam admon.textlabel 1
                --stringparam admon.graphics 0
                ${ASCIIDOC_MANPAGE_XSL}
                ${docbookxml}
            DEPENDS ${src} ${ARGN}
            )
    endif()
    install(FILES ${dst} DESTINATION "${MANDIR}/man${man_nr}")
endfunction()

function(gen_html sourcefile)
    # additional arguments (${ARGN}) are passed as DEPENDS to the asciidoc command
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${sourcefile}.txt")
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.html")

    # as a hack, every html doc depends on gen_object_asciidoc, even though only
    # 'herbstluftwm' needs it. We do this to avoid that hlwm-doc.json is built
    # multiple times when make is invoked with -j8
    # 'herbstluftwm' needs it
    add_custom_target("doc_html_${sourcefile}" ALL DEPENDS ${dst} gen_object_asciidoc)
    add_dependencies(all_doc "doc_html_${sourcefile}")
    if (COPY_DOCUMENTATION)
        set(dst_prebuilt "${CMAKE_CURRENT_SOURCE_DIR}/${sourcefile}.html")
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E copy ${dst_prebuilt} ${dst}
            DEPENDS ${dst_prebuilt}
            COMMENT "Using pre-built ${dst_prebuilt}"
            )
    else()
        add_custom_command(
            OUTPUT ${dst}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${ASCIIDOC}
                        -o ${dst}
                        -a \"builddir=${CMAKE_CURRENT_BINARY_DIR}\"
                        ${src}
            DEPENDS ${src} ${ARGN}
            )
    endif()
    install(FILES ${dst} DESTINATION "${DOCDIR}/html")
endfunction()


# always create a doc_json target, but only make it mandatory
# if WITH_DOCUMENTATION is set
gen_json_doc(hlwm-doc)

if (WITH_DOCUMENTATION)
    if (NOT COPY_DOCUMENTATION)
        find_program(ASCIIDOC_A2X NAMES a2x DOC "Path to AsciiDoc a2x command")
        find_program(ASCIIDOC NAMES asciidoc DOC "Path to AsciiDoc command")
        find_program(XSLTPROC NAMES xsltproc DOC "Path to xsltproc")
        set(ASCIIDOC_MANPAGE_XSL ${CMAKE_INSTALL_SYSCONF_PREFIX}/asciidoc/docbook-xsl/manpage.xsl
            CACHE PATH "The manpage.xsl provided by asciidoc to generate man pages from docbook xml")
    endif()

    gen_manpage(herbstclient 1)
    gen_manpage(herbstluftwm 1 "${CMAKE_CURRENT_BINARY_DIR}/hlwm-objects-gen.txt")
    gen_manpage(herbstluftwm-tutorial 7)

    gen_html(herbstclient)
    gen_html(herbstluftwm "${CMAKE_CURRENT_BINARY_DIR}/hlwm-objects-gen.txt")
    gen_html(herbstluftwm-tutorial)

    gen_object_asciidoc("hlwm-doc.json" "hlwm-objects-gen.txt")
endif()

# vim: et:ts=4:sw=4
