cmake_minimum_required(VERSION 3.20)

project(CircusCharlieHD VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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

if(WIN32)
  set_target_properties(circus_charlie_hd PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Keep a fresh copy of the assets next to the binary (and inside the macOS
# bundle). The old copy is removed first so renamed or deleted assets do not
# linger in the build tree.
add_custom_command(TARGET circus_charlie_hd POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:circus_charlie_hd>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:circus_charlie_hd>/assets
)

# The copy above only runs when the binary relinks; this target runs on
# every build, so an asset-only change (a new atlas, a new WAV) reaches the
# build tree and the bundle too.
add_custom_target(sync_assets ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets
          ${CMAKE_BINARY_DIR}/assets
  COMMENT "Syncing assets"
)
if(APPLE)
  add_custom_command(TARGET sync_assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/Resources/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets
            "${CMAKE_BINARY_DIR}/Circus Charlie HD.app/Contents/Resources/assets"
  )
endif()
add_dependencies(sync_assets circus_charlie_hd)

if(APPLE)
  set(APP_DIR "${CMAKE_BINARY_DIR}/Circus Charlie HD.app")
  add_custom_command(TARGET circus_charlie_hd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${APP_DIR}/Contents/Resources/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_DIR}/Contents/MacOS"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_DIR}/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:circus_charlie_hd>
            "${APP_DIR}/Contents/MacOS/circus_charlie_hd"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            "${APP_DIR}/Contents/Resources/assets"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/macos/Info.plist
            "${APP_DIR}/Contents/Info.plist"
  )
endif()

install(TARGETS circus_charlie_hd RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)
