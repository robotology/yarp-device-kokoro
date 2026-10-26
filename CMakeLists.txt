# SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(yarp-device-kokoro LANGUAGES CXX VERSION 0.1.0)

include(GNUInstallDirs)
find_package(YCM 0.15.3 REQUIRED)

set(YARP_FORCE_DYNAMIC_PLUGINS TRUE CACHE INTERNAL "yarp-device-kokoro uses a dynamic plugin")
option(BUILD_SHARED_LIBS "Build libraries as shared as opposed to static" ON)
option(BUILD_KOKORO_SMOKE_TEST "Build the standalone Kokoro smoke test (core YARP only)" ON)
find_package(YARP 4.0.0 COMPONENTS os sig dev REQUIRED)

find_package(YARP 4.0.0 COMPONENTS catch2 dev_tests QUIET)
if(YARP_catch2_FOUND AND YARP_dev_tests_FOUND)
  option(YARP_COMPILE_TESTS "Enable YARP dev tests" OFF)
  message(STATUS "yarp_catch2 and yarp_dev_tests libraries found")
else()
  set(YARP_COMPILE_TESTS OFF CACHE BOOL "YARP catch2/dev_tests not found; tests cannot be enabled" FORCE)
  message(STATUS "yarp_catch2 and yarp_dev_tests libraries NOT found. Tests cannot be enabled")
endif()

# enable_testing() must run regardless of whether YARP catch2/dev_tests are
# available: the standalone smoke test registers itself with CTest too.
if(YARP_COMPILE_TESTS OR BUILD_KOKORO_SMOKE_TEST)
  set(BUILD_TESTING ON)
  enable_testing()
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

include(AddInstallRPATHSupport)
add_install_rpath_support(BIN_DIRS "${CMAKE_INSTALL_FULL_BINDIR}"
                          LIB_DIRS "${CMAKE_INSTALL_FULL_LIBDIR}"
                          INSTALL_NAME_DIR "${CMAKE_INSTALL_FULL_LIBDIR}"
                          USE_LINK_PATH)

yarp_configure_plugins_installation(${PROJECT_NAME})
yarp_configure_external_installation(${PROJECT_NAME})

add_subdirectory(src)
add_subdirectory(tests)


include(FeatureSummary)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES)
