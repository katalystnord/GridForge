# Writes OUT, a C++ source defining the build stamp, from `git describe` in
# SRC. Run at every build (src/CMakeLists.txt); the file is rewritten only
# when the stamp changes, so an unchanged tree recompiles nothing.
execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${SRC} OUTPUT_VARIABLE build
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT build)
    set(build "unknown")
endif()
set(text "// Generated at build time by tools/build-stamp.cmake.\nnamespace gf::core { const char *buildStamp = \"${build}\"; }\n")
if(EXISTS ${OUT})
    file(READ ${OUT} old)
endif()
if(NOT old STREQUAL text)
    file(WRITE ${OUT} "${text}")
endif()
