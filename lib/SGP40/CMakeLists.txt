# Find all source files in a directory
aux_source_directory(. DIR_SGP40_SRCS)

include_directories(../Config)
# Adds a library target 
add_library(SGP40 ${DIR_SGP40_SRCS})
target_link_libraries(SGP40 PUBLIC Config)