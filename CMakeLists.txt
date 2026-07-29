cmake_minimum_required(VERSION 3.20)

project(BigTopRunNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(big_top_run
  src/main.cpp
)

target_compile_options(big_top_run PRIVATE
  $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(big_top_run PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)

if(TARGET SDL2::SDL2main)
  target_link_libraries(big_top_run PRIVATE SDL2::SDL2main)
endif()

add_custom_command(TARGET big_top_run POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:big_top_run>/assets
)

install(TARGETS big_top_run RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)
