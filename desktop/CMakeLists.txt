cmake_minimum_required(VERSION 3.16)
project(combocounter_desktop C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Add executable
add_executable(combocounter_desktop
    main.c
    ../embedded/simple_combo_core.c
)

# Include directories
target_include_directories(combocounter_desktop PRIVATE
    ../src
    ../embedded
    ${SDL2_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(combocounter_desktop
    ${SDL2_LIBRARIES}
    m
)

# Compiler flags
target_compile_options(combocounter_desktop PRIVATE
    ${SDL2_CFLAGS_OTHER}
    -Wall
    -Wextra
    -DCLAY_IMPLEMENTATION
)

# Debug/Release configurations
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(combocounter_desktop PRIVATE -g -O0)
else()
    target_compile_options(combocounter_desktop PRIVATE -O2)
endif()

# Install target
install(TARGETS combocounter_desktop
    RUNTIME DESTINATION bin
)