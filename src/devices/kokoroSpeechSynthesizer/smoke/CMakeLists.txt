# SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
# SPDX-License-Identifier: BSD-3-Clause
option(BUILD_KOKORO_SMOKE_TEST "Build the standalone Kokoro smoke test (no Catch2 required)" ON)
if(NOT BUILD_KOKORO_SMOKE_TEST)
  return()
endif()
add_executable(kokoroSmokeTest kokoro_smoke_test.cpp)
target_link_libraries(kokoroSmokeTest PRIVATE YARP::YARP_os YARP::YARP_sig YARP::YARP_dev YARP::YARP_init)
set_property(TARGET kokoroSmokeTest PROPERTY FOLDER "Example")
