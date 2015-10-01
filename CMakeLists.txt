project(build_bot)
cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules")

set(buildbot_VERSION_MAJOR 0)
set(buildbot_VERSION_MINOR 99)
set(buildbot_VERSION_PATCH 5)

find_package(Threads REQUIRED)

find_package(Boost REQUIRED
  COMPONENTS system thread log log_setup date_time chrono filesystem program_options random regex)
include_directories(${Boost_INCLUDE_DIRS})
if(NOT BOOST_USE_STATIC_LIBS)
  add_definitions(-DBOOST_ALL_DYN_LINK)
endif(NOT BOOST_USE_STATIC_LIBS)

set(dsnutil_cpp_BUILD_AS_SUBMODULE ON CACHE BOOL "")
set(dsnutil_cpp_BUILD_DOCS OFF CACHE BOOL "")
set(dsnutil_cpp_WITH_BASE64 OFF CACHE BOOL "")
set(dsnutil_cpp_WITH_EXAMPLES OFF CACHE BOOL "")
set(dsnutil_cpp_WITH_BOOST_LOG ON CACHE BOOL "")
add_subdirectory(dsnutil_cpp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dsnutil_cpp/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/dsnutil_cpp/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

configure_file(version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

file(GLOB buildbot_SRCS src/*.cpp)
list(APPEND buildbot_SRCS ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
add_executable(build_bot ${buildbot_SRCS})
target_compile_features(build_bot PRIVATE cxx_generalized_initializers cxx_strong_enums)
target_link_libraries(build_bot ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(build_bot dsnutil_cpp dsnutil_cpp-log)
target_link_libraries(build_bot ${Boost_LIBRARIES})
install(TARGETS build_bot RUNTIME DESTINATION bin)

set(CPACK_PACKAGE_VERSION_MAJOR ${buildbot_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${buildbot_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${buildbot_VERSION_PATCH})
include(CPack)

install(DIRECTORY include/build-bot DESTINATION include)
install(DIRECTORY etc/build-bot DESTINATION share/examples/)

include(CTest)
