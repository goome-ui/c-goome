cmake_minimum_required(VERSION 3.16)
project(goome C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GOOME_EXAMPLES "build the examples" ON)
option(GOOME_TESTS "build the tests" ON)
option(GOOME_NO_DRAW "leave the drawing layer and fonts out" OFF)

file(GLOB GOOME_SRC CONFIGURE_DEPENDS src/*.c)
add_library(goome STATIC ${GOOME_SRC})
target_include_directories(goome PUBLIC include PRIVATE src)
if(GOOME_NO_DRAW)
  target_compile_definitions(goome PUBLIC GOOME_NO_DRAW)
endif()
if(MSVC)
  target_compile_options(goome PRIVATE /W3 /wd4996)
else()
  target_compile_options(goome PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
if(UNIX AND NOT APPLE)
  target_link_libraries(goome PUBLIC ${CMAKE_DL_LIBS}) # only matters on glibc older than 2.34
endif()

add_custom_target(goome_single
  COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/tools/single.py
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "remaking goome.h")

if(GOOME_EXAMPLES)
  foreach(ex hello grab ui)
    add_executable(${ex} examples/${ex}.c)
    target_link_libraries(${ex} PRIVATE goome)
  endforeach()
endif()

if(GOOME_TESTS)
  enable_testing()
  add_executable(unit tests/unit.c)
  target_link_libraries(unit PRIVATE goome)
  add_test(NAME unit COMMAND unit)
endif()
