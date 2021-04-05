file(GLOB_RECURSE CFILES *.c)
file(GLOB_RECURSE HFILES *.h)

add_executable(baka_dns ${CFILES} ${HFILES})