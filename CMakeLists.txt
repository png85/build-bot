project(build-bot)
cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules")

set(buildbot_VERSION_MAJOR 0)
set(buildbot_VERSION_MINOR 99)
set(buildbot_VERSION_PATCH 0)

find_package(Threads REQUIRED)

find_package(Boost REQUIRED
  COMPONENTS system thread log log_setup date_time chrono filesystem program_options random regex)
include_directories(${Boost_INCLUDE_DIRS})
if(NOT BOOST_USE_STATIC_LIBS)
  add_definitions(-DBOOST_ALL_DYN_LINK)
endif(NOT BOOST_USE_STATIC_LIBS)
