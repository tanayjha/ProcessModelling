cmake_minimum_required(VERSION 3.16)
project(umpnap CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()

# ---- Engine (Qt-free, independently testable) ----
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/core/*.cpp
  ${CMAKE_SOURCE_DIR}/src/solver/*.cpp
  ${CMAKE_SOURCE_DIR}/src/components/*.cpp)
add_library(umpnap_engine STATIC ${ENGINE_SOURCES})
target_include_directories(umpnap_engine PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_subdirectory(tests)

# ---- GUI (optional; needs Qt6) ----
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
  file(GLOB GUI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/*.h)
  add_executable(umpnap ${GUI_SOURCES})
  target_link_libraries(umpnap PRIVATE umpnap_engine Qt6::Widgets)
  message(STATUS "Qt6 found - building GUI app 'umpnap'")
else()
  message(STATUS "Qt6 not found - building engine + tests only")
endif()
