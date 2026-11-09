# The build's git hash and configure date, for the logo hover, Settings and
# the saved state (src/BuildInfo.h). Written at configure time into a small
# generated header (only the files that include BuildInfo.h rebuild when it
# changes) and into build-label.txt next to the build, for scripts to print.
# With no git (a source zip), the hash is "unknown".

set(ILANA_BUILD_HASH "unknown")
find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    OUTPUT_VARIABLE ilana_git_hash
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE ilana_git_result)
    if (ilana_git_result EQUAL 0 AND NOT ilana_git_hash STREQUAL "")
        set(ILANA_BUILD_HASH "${ilana_git_hash}")

        # Configure again when HEAD moves (a commit, a checkout or a pull), so an
        # IDE build doesn't keep the old hash. Worktrees keep HEAD and the refs
        # in different folders: git names the files.
        foreach (ilana_git_path HEAD packed-refs)
            execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path ${ilana_git_path}
                            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                            OUTPUT_VARIABLE ilana_git_file OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            get_filename_component(ilana_git_file "${ilana_git_file}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
            if (EXISTS "${ilana_git_file}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ilana_git_file}")
            endif()
        endforeach()
        execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        OUTPUT_VARIABLE ilana_git_ref OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (NOT ilana_git_ref STREQUAL "")
            execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${ilana_git_ref}"
                            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                            OUTPUT_VARIABLE ilana_git_file OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            get_filename_component(ilana_git_file "${ilana_git_file}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
            if (EXISTS "${ilana_git_file}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ilana_git_file}")
            endif()
        endif()
    endif()
endif()

string(TIMESTAMP ILANA_BUILD_DATE "%Y-%m-%d")
# Plain ASCII in the text file, so a Windows console prints it as it is
# (the plugin shows "hash · date", BuildInfo.h).
set(ILANA_BUILD_LABEL "${ILANA_BUILD_HASH} - ${ILANA_BUILD_DATE}")

set(ILANA_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_LIST_DIR}/BuildInfoGenerated.h.in" "${ILANA_GENERATED_DIR}/BuildInfoGenerated.h" @ONLY)
file(WRITE "${CMAKE_BINARY_DIR}/build-label.txt" "${ILANA_BUILD_LABEL}\n")
message(STATUS "ilanaSynth build: ${ILANA_BUILD_LABEL}")
