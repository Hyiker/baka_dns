file(GLOB_RECURSE CFILES *.c)
file(GLOB_RECURSE HFILES *.h)

add_executable(baka_dns ${CFILES} ${HFILES})
if(APPLE)
    find_library(ARGP_LIB argp)
    target_link_libraries(baka_dns PUBLIC ${ARGP_LIB})
endif(APPLE)
