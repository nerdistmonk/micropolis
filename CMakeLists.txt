cmake_minimum_required(VERSION 3.20)
project(micropolis_sdl3 C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Compile flags & optimization
add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)

# Auto-discover all source files across core/ and sdl3/
# CONFIGURE_DEPENDS ensures CMake re-runs automatically when files are added or deleted
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.c"
)

file(GLOB_RECURSE SDL3_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/*.c"
)

# Main executable target
add_executable(micropolis_sdl3
    ${CORE_SOURCES}
    ${SDL3_SOURCES}
)

# Header include paths
target_include_directories(micropolis_sdl3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/headers
    ${CMAKE_CURRENT_SOURCE_DIR}/sdl3
    ${CMAKE_CURRENT_SOURCE_DIR}/sdl3/headers
)

# Required dependencies: SDL3, SDL3_ttf, and standard math library
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(SDL3_mixer REQUIRED CONFIG)

target_link_libraries(micropolis_sdl3 PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
    m
)

# Copy runtime assets adjacent to the binary in the build directory
add_custom_command(TARGET micropolis_sdl3 POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/fonts"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/music"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/music"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/images"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/images"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/scenarios"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/sounds"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/sounds"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/cities"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/cities"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/manual"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/manual"
   COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/music"
        "$<TARGET_FILE_DIR:micropolis_sdl3>/music"
    COMMENT "Synchronizing game assets to build directory"
)
