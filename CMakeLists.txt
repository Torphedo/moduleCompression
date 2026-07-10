cmake_minimum_required(VERSION 3.13)
project(moduleCompression
    LANGUAGES C CXX
)
# We need compile_commands.json for the include paths in some editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# Put all binaries we'll need to distribute in 1 folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Force shared libraries to load from a relative path instead of hardcoding
# their path on my computer (why is this the default?)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)


set(CMAKE_C_STANDARD 11)

include_directories(
    lib
    lib/ma_trackerFormats
    lib/miniaudio
)

add_subdirectory(lib/bobtail)
add_subdirectory(lib/opus)
add_subdirectory(lib/opusfile)
add_subdirectory(lib/vorbis)

add_library(opusenc STATIC
    lib/opusenc/src/ogg_packer.c
    lib/opusenc/src/opus_header.c
    lib/opusenc/src/opusenc.c
    lib/opusenc/src/picture.c
    lib/opusenc/src/random.c
    lib/opusenc/src/resample.c
    lib/opusenc/src/unicode_support.c
)
target_include_directories(opusenc PUBLIC lib/opusenc/include)
target_link_libraries(opusenc PUBLIC opus)
target_compile_definitions(opusenc PUBLIC RANDOM_PREFIX=libopusenc OUTSIDE_SPEEX PACKAGE_NAME="moduleCompression" PACKAGE_VERSION="1.0")

add_library(miniaudio STATIC
    lib/miniaudio/miniaudio.c
    lib/miniaudio/extras/decoders/libvorbis/miniaudio_libvorbis.c
    lib/miniaudio/extras/decoders/libopus/miniaudio_libopus.c
)
target_include_directories(miniaudio PUBLIC
    lib/miniaudio
    lib/miniaudio/extras/decoders/libvorbis
    lib/miniaudio/extras/decoders/libopus
)
target_compile_definitions(miniaudio PUBLIC
    MA_NO_NODE_GRAPH MA_NO_RESOURCE_MANAGER
    MA_NO_GENERATION MA_NO_ENCODING MA_NO_FLAC
    MA_NO_ENGINE
)
target_link_libraries(miniaudio PUBLIC opusfile vorbisfile)

# Link this for Miniaudio with tracker module support
add_subdirectory(lib/ma_trackerFormats)
add_library(ma_trackerFormats STATIC
    lib/ma_trackerFormats/ibxm_reader.c
    lib/ma_trackerFormats/miniaudio_ibxm.c
    lib/ma_trackerFormats/miniaudio_it2play.c
)
target_link_libraries(ma_trackerFormats PUBLIC miniaudio ibxm IT2)
target_include_directories(miniaudio PUBLIC lib/ma_trackerFormats lib/ma_trackerFormats/lib/it2play)

# We only need to explictly link with the math library (just "m") on Linux.
set(MATH_LIB "m")
if(MSVC)
    set(MATH_LIB "")
endif()

add_executable(modTool
    src/main.c
    src/rewriteMOD.c
    src/rewriteXM.c
    src/rewriteS3M.c
    src/pcm.c
    src/VirtualIO.c
)

target_link_libraries(modTool PRIVATE ma_trackerFormats bobtail opusenc ${MATH_LIB})
