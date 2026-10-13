/*
 * SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <yarp/dev/ISpeechSynthesizer.h>
#include <yarp/os/Network.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/WrapperSingle.h>

#include <yarp/sig/Sound.h>
#include <yarp/sig/SoundFile.h>

#include <catch2/catch_amalgamated.hpp>
#include <harness.h>

using namespace yarp::dev;
using namespace yarp::os;

TEST_CASE("dev::kokoroSpeechSynthesizer_test", "[yarp::dev]")
{
    YARP_REQUIRE_PLUGIN("kokoroSpeechSynthesizer", "device");

    Network::setLocalMode(true);

    SECTION("Checking kokoroSpeechSynthesizer device")
    {
        ISpeechSynthesizer* iSynth{nullptr};
        PolyDriver dd;

        //"Checking opening device"
        {
            Property pcfg;
            pcfg.put("device", "kokoroSpeechSynthesizer");
            pcfg.put("language", "en");
            pcfg.put("voice", "auto");
            pcfg.put("speed", 1.0);
            REQUIRE(dd.open(pcfg));
            REQUIRE(dd.view(iSynth));
        }

        // Interface-level state checks (fast; keep the model out of it).
        const std::string lang_to_set{"en-gb"};
        const std::string voice_to_set{"bf_emma"};
        std::string lang_code;
        std::string voice_name;

        CHECK(iSynth->setLanguage(lang_to_set));
        CHECK(iSynth->getLanguage(lang_code));
        CHECK(lang_code == lang_to_set);

        // Put the language back to a stable default before continuing.
        CHECK(iSynth->setLanguage("en"));
        CHECK(iSynth->getLanguage(lang_code));
        CHECK(lang_code == "en");

        CHECK(iSynth->setVoice(voice_to_set));
        CHECK(iSynth->getVoice(voice_name));
        CHECK(voice_name == voice_to_set);

        double speed = 0.0;
        CHECK(iSynth->setSpeed(1.5));
        CHECK(iSynth->getSpeed(speed));
        CHECK(speed == 1.5);

        double pitch = 1.0;
        CHECK(iSynth->setPitch(0.0));
        CHECK(iSynth->getPitch(pitch));
        CHECK(pitch == 0.0);

        // Error handling: invalid ranges and unknown language must fail.
        CHECK_FALSE(iSynth->setSpeed(0.0));
        CHECK_FALSE(iSynth->setSpeed(5.0));
        CHECK_FALSE(iSynth->setLanguage("zz"));

        // The synthesis test is heavy (downloads/loads the Kokoro model), so it
        // is opt-in via KOKORO_TEST_SYNTHESIZE.
        const char* synthFlag = std::getenv("KOKORO_TEST_SYNTHESIZE");
        if (!(synthFlag != nullptr && std::string(synthFlag) == "1")) {
            // YARP_SKIP_TEST marks the test skipped; the goto keeps the compiler
            // happy about the label while being unreachable at runtime.
            YARP_SKIP_TEST("Kokoro synthesis test skipped (set KOKORO_TEST_SYNTHESIZE=1 to enable)");
            goto skip_synthesis;
        }
        {
            const std::string toSynthesize{"This is a Kokoro synthesis test from YARP"};
            yarp::sig::Sound outputSound;
            CHECK(iSynth->synthesize(toSynthesize, outputSound));
            CHECK(outputSound.getSamples() > 0);
            CHECK(outputSound.getFrequency() > 0);
        }
skip_synthesis:

        //"Close all polydrivers and check"
        {
            CHECK(dd.close());
        }
    }

    Network::setLocalMode(false);
}
