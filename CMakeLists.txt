cmake_minimum_required(VERSION 3.16)

project(ufedm-dev
    VERSION 1.0.0
    LANGUAGES C
)

set(LINUX_MTD_SOURCES linux-mtd/dev.c)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(EXTENDED_SOURCES linux-mtd/dev.c)
endif()

# Build static library
add_library(ufedm-dev STATIC
    proxy_dev.c
    emulated-proxy/dev.c
    emulated-proxy/bitmap.c
    emulated-proxy/bits_array.c
    emulated-proxy/sync_queue.c

    ${EXTENDED_SOURCES}
)

target_include_directories(ufedm-dev PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Require C11
target_compile_features(ufedm-dev PUBLIC c_std_11)

# Have an install directive, for users that might need it...
install(TARGETS ufedm-dev
    ARCHIVE DESTINATION lib
)

