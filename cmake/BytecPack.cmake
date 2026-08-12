function(bytec_pack_folder)
    set(one_value_args TARGET FOLDER OUTPUT)
    cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})

    if(NOT ARG_TARGET OR NOT ARG_FOLDER OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "bytec_pack_folder: TARGET, FOLDER and OUTPUT are required")
    endif()

    get_filename_component(_output_dir "${ARG_OUTPUT}" DIRECTORY)

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_output_dir}"
        COMMAND $<TARGET_FILE:bytec_cli> pack "${ARG_FOLDER}" "${ARG_OUTPUT}"
        DEPENDS bytec_cli
        VERBATIM
    )

    add_custom_target(${ARG_TARGET} ALL DEPENDS "${ARG_OUTPUT}")
endfunction()

function(bytec_embed_folder)
    set(one_value_args TARGET FOLDER HEADER SYMBOL)
    cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})

    if(NOT ARG_TARGET OR NOT ARG_FOLDER OR NOT ARG_HEADER OR NOT ARG_SYMBOL)
        message(FATAL_ERROR "bytec_embed_folder: TARGET, FOLDER, HEADER and SYMBOL are required")
    endif()

    get_filename_component(_header_dir "${ARG_HEADER}" DIRECTORY)
    set(_archive "${ARG_HEADER}.byc")

    add_custom_command(
        OUTPUT "${ARG_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_header_dir}"
        COMMAND $<TARGET_FILE:bytec_cli> pack "${ARG_FOLDER}" "${_archive}"
        COMMAND $<TARGET_FILE:bytec_cli> embed "${_archive}" "${ARG_HEADER}" "${ARG_SYMBOL}"
        DEPENDS bytec_cli
        VERBATIM
    )

    add_custom_target(${ARG_TARGET}_generate DEPENDS "${ARG_HEADER}")
    add_library(${ARG_TARGET} INTERFACE)
    add_dependencies(${ARG_TARGET} ${ARG_TARGET}_generate)
    target_include_directories(${ARG_TARGET} INTERFACE "${_header_dir}")
endfunction()
