# Find all source files in a directory
aux_source_directory(. DIR_SHTC3_SRCS)

include_directories(../Config)
# Adds a library target 
add_library(SHTC3 ${DIR_SHTC3_SRCS})
target_link_libraries(SHTC3 PUBLIC Config)