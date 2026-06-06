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

# ---- Example generator (engine-only helper) ----
add_executable(make_example examples/make_loop.cpp)
target_link_libraries(make_example PRIVATE umpnap_engine)
add_executable(make_drain examples/make_drain.cpp)
target_link_libraries(make_drain PRIVATE umpnap_engine)

# ---- GUI (optional; needs Qt6) ----
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
  # GUI widgets/scene as a library, reused by the app and the screenshot tool.
  file(GLOB GUI_LIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/gui/*.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/*.h)
  list(FILTER GUI_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
  add_library(umpnap_gui STATIC ${GUI_LIB_SOURCES})
  target_link_libraries(umpnap_gui PUBLIC umpnap_engine Qt6::Widgets)

  add_executable(umpnap src/gui/main.cpp)
  target_link_libraries(umpnap PRIVATE umpnap_gui)
  message(STATUS "Qt6 found - building GUI app 'umpnap'")

  add_executable(umpnap_screenshot tools/screenshot.cpp)
  target_link_libraries(umpnap_screenshot PRIVATE umpnap_gui)
else()
  message(STATUS "Qt6 not found - building engine + tests only")
endif()

if(Qt6_FOUND)
  add_executable(umpnap_trendcheck tools/trend_check.cpp)
  target_link_libraries(umpnap_trendcheck PRIVATE umpnap_gui)
endif()
