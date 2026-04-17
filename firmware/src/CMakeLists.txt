# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS
        "main.c"
        "DPS.c"
        "SD.c"
        "collate.c"
        "GPS.c"
        "GPS_parser.c"
        "screen.c"
        "BatMGMT.c"
        "logger.c"
    INCLUDE_DIRS
        "."
        "../include"
)