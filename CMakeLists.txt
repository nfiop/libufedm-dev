cmake_minimum_required(VERSION 3.16)

project(ufedmdev
    VERSION 1.0.0
    LANGUAGES C
)

# Build static library
add_library(ufedmdev STATIC
    proxy_dev.c
)

target_include_directories(ufedmdev PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Require C11
target_compile_features(ufedmdev PUBLIC c_std_11)

# Have an install directive, for users that might need it...
install(TARGETS ufedmdev
    ARCHIVE DESTINATION lib
)

