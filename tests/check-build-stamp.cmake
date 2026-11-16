# The build every program reports is the source it was built from: the
# version line must carry what `git describe` says now, not what it said
# when CMake last configured (the files GridForge writes carry it as
# provenance). Run as: cmake -DCLI=... -DSRC=... -P check-build-stamp.cmake
execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${SRC} OUTPUT_VARIABLE now
                OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE git_failed)
if(git_failed)
    message(STATUS "not a git checkout; nothing to compare")
    return()
endif()
execute_process(COMMAND ${CLI} --version OUTPUT_VARIABLE line
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT line MATCHES "\\(build ${now}\\)$")
    message(FATAL_ERROR "reports \"${line}\", but the source is ${now}")
endif()
message(STATUS "build ${now}")
