cmake_minimum_required(VERSION 3.22)

# Kindred -- chord progression and arrangement generator (AU MIDI FX, VST3, Standalone).
# The product name, codes and bundle id are set in ONE place: src/Product.h.
# Version and build number: src/Product.h as well (KINDRED_VERSION, KINDRED_BUILD).
function(kindred_define out name)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/Product.h" line REGEX "#define ${name} ")
  string(REGEX REPLACE ".*#define ${name} +\"?([^\"]*)\"?.*" "\\1" value "${line}")
  if(value STREQUAL "")
    message(FATAL_ERROR "Could not read ${name} from src/Product.h")
  endif()
  set(${out} "${value}" PARENT_SCOPE)
endfunction()
kindred_define(KINDRED_PRODUCT_NAME KINDRED_PRODUCT_NAME)
kindred_define(KINDRED_TARGET KINDRED_TARGET_NAME)
kindred_define(KINDRED_COMPANY KINDRED_COMPANY_NAME)
kindred_define(KINDRED_MFR KINDRED_MANUFACTURER_CODE)
kindred_define(KINDRED_CODE KINDRED_PLUGIN_CODE)
kindred_define(KINDRED_BUNDLE KINDRED_BUNDLE_ID)
kindred_define(KINDRED_VERSION KINDRED_VERSION)
kindred_define(KINDRED_BUILD KINDRED_BUILD)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Product.h")

# macOS: one universal binary (Intel and Apple silicon) that runs on macOS 10.13 and later.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment target")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures")
# Windows: link the C++ runtime into the binaries, so that no redistributable package is needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(${KINDRED_TARGET} VERSION ${KINDRED_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  # The sources are UTF-8 (note names with flat and sharp signs, arrows in the chord chart).
  add_compile_options(/utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
  # A call to a system function newer than the deployment target, without an availability
  # check, stops the build. This is the proof that the binary can run on the oldest system.
  add_compile_options(-Werror=unguarded-availability-new -Werror=unguarded-availability)
endif()
# The engine must do its arithmetic exactly as the JavaScript prototype did (no fused multiply-add),
# or a roll can differ from the reference by one unit in the last place and pick another chord.
if(MSVC)
  set(KINDRED_EXACT_FP /fp:strict)
else()
  set(KINDRED_EXACT_FP -ffp-contract=off)
endif()

# ---- engine core: pure C++17, no JUCE ----------------------------------------------------------
add_library(kindred_engine STATIC
  src/engine/Json.cpp src/engine/Style.cpp src/engine/Engine.cpp src/engine/EngineIO.cpp)
target_include_directories(kindred_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(kindred_engine PRIVATE ${KINDRED_EXACT_FP})
if(MSVC)
  target_compile_options(kindred_engine PRIVATE /W4)
else()
  target_compile_options(kindred_engine PRIVATE -Wall -Wextra)
endif()

add_executable(KindredGolden tests/golden/GoldenTest.cpp)
target_link_libraries(KindredGolden PRIVATE kindred_engine)
add_executable(KindredRules tests/RulesTest.cpp)
target_link_libraries(KindredRules PRIVATE kindred_engine)
target_compile_options(KindredRules PRIVATE ${KINDRED_EXACT_FP})

add_executable(KindredPlayerTest tests/PlayerTest.cpp)
target_link_libraries(KindredPlayerTest PRIVATE kindred_engine)

option(KINDRED_ENGINE_ONLY "Build only the engine library and its tests (no JUCE)" OFF)
if(KINDRED_ENGINE_ONLY)
  return()
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Plugin.cmake)
