# SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
# SPDX-License-Identifier: BSD-3-Clause

yarp_prepare_plugin(kokoroSpeechSynthesizer
  CATEGORY device
  TYPE KokoroSpeechSynthesizer
  INCLUDE KokoroSpeechSynthesizer.h
  INTERNAL ON
)

if(NOT SKIP_kokoroSpeechSynthesizer)
  yarp_add_plugin(yarp_kokoroSpeechSynthesizer)
  generateDeviceParamsParser(KokoroSpeechSynthesizer kokoroSpeechSynthesizer)

  target_sources(yarp_kokoroSpeechSynthesizer
    PRIVATE
      KokoroSpeechSynthesizer.cpp
      KokoroSpeechSynthesizer.h
      KokoroSpeechSynthesizer_ParamsParser.cpp
      KokoroSpeechSynthesizer_ParamsParser.h
  )

  target_compile_definitions(yarp_kokoroSpeechSynthesizer
    PRIVATE
      KOKORO_WORKER_SCRIPT_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/kokoro_worker.py"
      KOKORO_WORKER_SCRIPT_INSTALL="${CMAKE_INSTALL_FULL_DATADIR}/yarp-kokoro/kokoro_worker.py"
  )

  target_link_libraries(yarp_kokoroSpeechSynthesizer
    PRIVATE
      YARP::YARP_os
      YARP::YARP_sig
      YARP::YARP_dev
  )

  yarp_install(
    TARGETS yarp_kokoroSpeechSynthesizer
    EXPORT yarp-device-kokoroSpeechSynthesizer
    COMPONENT yarp-device-kokoroSpeechSynthesizer
    LIBRARY DESTINATION ${YARP_DYNAMIC_PLUGINS_INSTALL_DIR}
    ARCHIVE DESTINATION ${YARP_STATIC_PLUGINS_INSTALL_DIR}
    YARP_INI DESTINATION ${YARP_PLUGIN_MANIFESTS_INSTALL_DIR}
  )

  install(FILES kokoro_worker.py
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yarp-kokoro
    COMPONENT yarp-device-kokoroSpeechSynthesizer
  )

  add_subdirectory(smoke)

  set_property(TARGET yarp_kokoroSpeechSynthesizer PROPERTY FOLDER "Plugins/Device")
endif()
