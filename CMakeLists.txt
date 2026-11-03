cmake_minimum_required(VERSION 3.25)
project(micropolis_sdl3 LANGUAGES C)

# Modern C standard configuration
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict warnings to prevent undefined behavior and legacy implicit declarations
add_compile_options(
  -Wall
  -Wextra
  -Wstrict-prototypes
  -Wmissing-prototypes
  -Wno-unused-parameter
  -Wno-sign-compare
  -O2
)

# Core engine source tree
set(SIM_CORE_SOURCES
  core/s_alloc.c
  core/s_sim.c
  core/s_zone.c
  core/s_power.c
  core/s_traf.c
  core/s_scan.c
  core/s_gen.c
  core/s_eval.c
  core/s_disast.c
  core/s_fileio.c
  core/s_msg.c
  core/s_init.c
  core/s_con.c
  core/s_sprite.c
)

# Static simulation core library
add_library(sim_core STATIC ${SIM_CORE_SOURCES})
target_include_directories(sim_core PUBLIC core/headers)
target_link_libraries(sim_core PRIVATE m)

# Find system SDL3 and SDL3_ttf on Arch Linux
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

# Modular SDL3 Frontend Sources
set(SIM_SDL3_SOURCES
  sdl3/main.c
  sdl3/sdl3_video.c
  sdl3/sdl3_tiles.c
  sdl3/sdl3_tiles_data.c
  sdl3/sdl3_gui.c
  sdl3/sdl3_gui_text.c
  sdl3/sdl3_gui_dialogs.c
  sdl3/sdl3_input.c
  sdl3/sdl3_audio.c
)

# Graphical SDL3 application executable
add_executable(micropolis_sdl3
  ${SIM_SDL3_SOURCES}
)
target_include_directories(micropolis_sdl3 PRIVATE core/headers sdl3/headers)
target_link_libraries(micropolis_sdl3 PRIVATE sim_core SDL3::SDL3 SDL3_ttf::SDL3_ttf m)