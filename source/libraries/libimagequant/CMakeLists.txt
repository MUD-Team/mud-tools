
set (LIBIMAGEQUANT_SOURCES blur.c kmeans.c libimagequant.c mediancut.c mempool.c nearest.c pam.c remap.c)

add_library(libimagequant STATIC ${LIBIMAGEQUANT_SOURCES})

target_include_directories(libimagequant PUBLIC .)