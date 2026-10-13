/*
 * SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Self-contained smoke test for the Kokoro YARP device. It does NOT depend on
// Catch2 or the YARP dev_tests component; it only needs the core YARP
// libraries (os, sig, dev, init). It exercises the full open + set/get +
// synthesize path, so it actually loads and runs the Kokoro model.

#include <yarp/os/Property.h>
#include <yarp/os/Network.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/ISpeechSynthesizer.h>
#include <yarp/sig/Sound.h>

#include <iostream>
#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    yarp::os::NetworkBase::initMinimum();

    // Optional CLI arguments: [language] [voice]
    std::string language = "en";
    std::string voice = "auto";
    if (argc > 1) {
        language = argv[1];
    }
    if (argc > 2) {
        voice = argv[2];
    }

    yarp::os::Property cfg;
    cfg.put("device", "kokoroSpeechSynthesizer");
    cfg.put("language", language);
    cfg.put("voice", voice);
    cfg.put("speed", 1.0);
    cfg.put("pitch", 0.0);

    const char* kokoroPython = std::getenv("KOKORO_PYTHON");
    if (kokoroPython != nullptr && *kokoroPython != '\0') {
        cfg.put("python_executable", std::string(kokoroPython));
    }

    yarp::dev::PolyDriver dd;
    if (!dd.open(cfg)) {
        std::cerr << "ERROR: Failed to open kokoroSpeechSynthesizer device" << std::endl;
        return 1;
    }

    yarp::dev::ISpeechSynthesizer* iSynth = nullptr;
    if (!dd.view(iSynth) || iSynth == nullptr) {
        std::cerr << "ERROR: Failed to view ISpeechSynthesizer interface" << std::endl;
        dd.close();
        return 1;
    }

    // Interface-level checks that do not need the model.
    {
        std::string lang;
        if (!iSynth->getLanguage(lang)) {
            std::cerr << "ERROR: getLanguage failed" << std::endl;
            dd.close();
            return 1;
        }
        std::cout << "getLanguage -> " << lang << std::endl;

        if (!iSynth->setLanguage(language)) {
            std::cerr << "ERROR: setLanguage(" << language << ") failed" << std::endl;
            dd.close();
            return 1;
        }
        if (!iSynth->getLanguage(lang) || lang != language) {
            std::cerr << "ERROR: getLanguage returned unexpected value: " << lang << std::endl;
            dd.close();
            return 1;
        }
    }

    {
        std::string v;
        if (!iSynth->setVoice(voice)) {
            std::cerr << "ERROR: setVoice(" << voice << ") failed" << std::endl;
            dd.close();
            return 1;
        }
        if (!iSynth->getVoice(v)) {
            std::cerr << "ERROR: getVoice failed" << std::endl;
            dd.close();
            return 1;
        }
        std::cout << "getVoice -> " << v << std::endl;
    }

    {
        double speed = 0.0;
        if (!iSynth->setSpeed(2.0) || !iSynth->getSpeed(speed)) {
            std::cerr << "ERROR: setSpeed/getSpeed failed" << std::endl;
            dd.close();
            return 1;
        }
        std::cout << "setSpeed(2.0) -> getSpeed " << speed << std::endl;
        if (speed != 2.0) {
            std::cerr << "ERROR: getSpeed returned unexpected value: " << speed << std::endl;
            dd.close();
            return 1;
        }
    }

    {
        double pitch = 1.0;
        if (!iSynth->setPitch(0.5) || !iSynth->getPitch(pitch)) {
            std::cerr << "ERROR: setPitch/getPitch failed" << std::endl;
            dd.close();
            return 1;
        }
        std::cout << "setPitch(0.5) -> getPitch " << pitch << std::endl;
    }

    // Now actually run the model.
    std::cout << "Synthesizing audio (this loads/runs the Kokoro model, may take a while)..." << std::endl;
    yarp::sig::Sound sound;
    if (!iSynth->synthesize("This is a Kokoro smoke test from YARP.", sound)) {
        std::cerr << "ERROR: synthesize failed" << std::endl;
        dd.close();
        return 1;
    }

    std::cout << "SYNTHESIZED freq=" << sound.getFrequency()
              << " samples=" << sound.getSamples()
              << " channels=" << sound.getChannels() << std::endl;

    dd.close();
    return 0;
}
