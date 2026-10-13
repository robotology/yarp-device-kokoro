# SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
# SPDX-License-Identifier: BSD-3-Clause

if(NOT BUILD_KOKORO_SMOKE_TEST)
  return()
endif()

add_executable(kokoroSmokeTest kokoro_smoke_test.cpp)
target_link_libraries(kokoroSmokeTest PRIVATE YARP::YARP_os YARP::YARP_sig YARP::YARP_dev YARP::YARP_init)
set_property(TARGET kokoroSmokeTest PROPERTY FOLDER "Example")

# Register the smoke test with CTest so it also runs from the build tree. It
# needs to find the dynamically loaded plugin (manifest dir + library path).
if(BUILD_TESTING)
  add_test(NAME kokoro_smoke_test COMMAND kokoroSmokeTest)
  set_tests_properties(kokoro_smoke_test PROPERTIES
    ENVIRONMENT "YARP_DATA_DIRS=${CMAKE_BINARY_DIR}/share/yarp;LD_LIBRARY_PATH=$<TARGET_FILE_DIR:YARP::YARP_os>:${CMAKE_BINARY_DIR}/lib/yarp:${CMAKE_BINARY_DIR}/lib"
    TIMEOUT 900)
endif()
