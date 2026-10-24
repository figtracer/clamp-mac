cmake_minimum_required(VERSION 3.14)
project(Clamp VERSION 1.1.0)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)

find_package(iPlug2 REQUIRED)

iplug_add_plugin(${PROJECT_NAME}
  FORMATS APP VST3 AU
  SOURCES
    Clamp.cpp
    Clamp.h
    ClampUI.cpp
    ClampUI.h
    Waveshapers.h
    resources/resource.h
  RESOURCES
    resources/fonts/Inter-Bold.ttf
    resources/fonts/Inter-Regular.ttf
    resources/fonts/Inter-SemiBold.ttf
)
