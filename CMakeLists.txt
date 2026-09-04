cmake_minimum_required(VERSION 3.20)

project(CircusCharlieHD VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(circus_charlie_hd
  src/main.cpp
)

target_compile_options(circus_charlie_hd PRIVATE
  $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(circus_charlie_hd PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)

if(TARGET SDL2::SDL2main)
  target_link_libraries(circus_charlie_hd PRIVATE SDL2::SDL2main)
endif()

add_custom_command(TARGET circus_charlie_hd POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:circus_charlie_hd>/assets
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/MacOS"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/Resources"
  COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:circus_charlie_hd>
          "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/MacOS/circus_charlie_hd"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/Resources/assets"
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/macos/Info.plist
          "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/Info.plist"
)

install(TARGETS circus_charlie_hd RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)
