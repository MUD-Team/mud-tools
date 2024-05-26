
set (LODEPNG_SOURCES lodepng.cpp lodepng_util.cpp)

add_library(lodepng STATIC ${LODEPNG_SOURCES})

target_include_directories(lodepng PUBLIC .)