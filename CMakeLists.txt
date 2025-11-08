cmake_minimum_required(VERSION 3.27)
project(combo_chracker C)
set(CMAKE_C_STANDARD 99)

# Adding Raylib
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
    GIT_TAG "4.5.0"
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib)

# Fetch Clay
FetchContent_Declare(
    clay
    GIT_REPOSITORY "https://github.com/nicbarker/clay.git"
    GIT_TAG "main"
)
FetchContent_GetProperties(clay)
if(NOT clay_POPULATED)
    FetchContent_Populate(clay)
endif()

# Add source files
set(SOURCES
    src/main.c
    src/core.c
    src/break_menu.c
    src/clay_renderer_raylib.c
    src/clay_impl.c
    src/ui.c
    src/input.c
    src/colors.c
    src/break_activities.c
    src/widgets.c
    src/forms.c
    src/timer.c
)

# Main executable
add_executable(combo_chracker ${SOURCES})

# Compiler options
target_compile_options(combo_chracker PUBLIC)
target_include_directories(combo_chracker PUBLIC
    .
    ${clay_SOURCE_DIR}
    ${clay_SOURCE_DIR}/renderers/raylib
    ${raylib_SOURCE_DIR}/src/extras
)

# Link libraries
target_link_libraries(combo_chracker PUBLIC raylib)

if(UNIX)
    target_link_libraries(combo_chracker PUBLIC m)
endif()

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -DCLAY_DEBUG")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

# Create resources directory if it doesn't exist
add_custom_command(
    TARGET combo_chracker PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_CURRENT_BINARY_DIR}/resources
)

# Copy resources if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(
        TARGET combo_chracker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${CMAKE_CURRENT_BINARY_DIR}/resources
    )
endif()
