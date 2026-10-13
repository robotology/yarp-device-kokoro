/*
 * SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef YARP_KOKOROSPEECHSYNTHESIZER_H
#define YARP_KOKOROSPEECHSYNTHESIZER_H

#include <yarp/dev/DeviceDriver.h>
#include <yarp/dev/ISpeechSynthesizer.h>
#include <yarp/sig/Sound.h>

#include <mutex>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#  include <sys/types.h>
#endif

/**
 * @ingroup dev_impl_other
 *
 * @brief YARP speech synthesizer backed by the Kokoro Python package.
 *
 * The plugin keeps a small Python worker alive and communicates with it through
 * stdin/stdout. This avoids embedding Python in the YARP process while still
 * keeping the Kokoro model loaded between synthesis requests.
 */
class KokoroSpeechSynthesizer final :
        public yarp::dev::DeviceDriver,
        public yarp::dev::ISpeechSynthesizer
{
public:
    KokoroSpeechSynthesizer() = default;
    KokoroSpeechSynthesizer(const KokoroSpeechSynthesizer&) = delete;
    KokoroSpeechSynthesizer(KokoroSpeechSynthesizer&&) = delete;
    KokoroSpeechSynthesizer& operator=(const KokoroSpeechSynthesizer&) = delete;
    KokoroSpeechSynthesizer& operator=(KokoroSpeechSynthesizer&&) = delete;
    ~KokoroSpeechSynthesizer() override;

    bool open(yarp::os::Searchable& config) override;
    bool close() override;

    yarp::dev::ReturnValue setLanguage(const std::string& language = "auto") override;
    yarp::dev::ReturnValue getLanguage(std::string& language) override;
    yarp::dev::ReturnValue setVoice(const std::string& voice_name = "auto") override;
    yarp::dev::ReturnValue getVoice(std::string& voice_name) override;
    yarp::dev::ReturnValue setSpeed(double speed = 0) override;
    yarp::dev::ReturnValue getSpeed(double& speed) override;
    yarp::dev::ReturnValue setPitch(double pitch) override;
    yarp::dev::ReturnValue getPitch(double& pitch) override;
    yarp::dev::ReturnValue synthesize(const std::string& text, yarp::sig::Sound& sound) override;

private:
    bool startWorker();
    void stopWorker();
    bool workerSynthesize(const std::string& text, yarp::sig::Sound& sound);

    static std::string base64Encode(const std::string& input);
    static bool base64Decode(const std::string& input, std::vector<unsigned char>& output);
    static bool knownLanguage(const std::string& language, std::string& kokoroCode);
    static std::string defaultVoiceFor(const std::string& kokoroCode);

#if defined(__unix__) || defined(__APPLE__)
    bool writeAll(const std::string& data);
    bool readLine(std::string& line, int timeoutMs);
    int m_workerInput{-1};
    int m_workerOutput{-1};
    pid_t m_workerPid{-1};
#endif

    std::mutex m_mutex;
    bool m_open{false};
    std::string m_pythonExecutable{"python3"};
    std::string m_workerScript;
    std::string m_modelRepository;
    std::string m_device;
    std::string m_language{"en"};
    std::string m_kokoroLanguage{"a"};
    std::string m_voice{"auto"};
    bool m_voiceExplicit{false};
    double m_speed{1.0};
    double m_pitch{0.0};
};

#endif // YARP_KOKOROSPEECHSYNTHESIZER_H
