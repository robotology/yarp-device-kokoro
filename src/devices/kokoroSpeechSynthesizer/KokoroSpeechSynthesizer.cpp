/*
 * SPDX-FileCopyrightText: 2026 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "KokoroSpeechSynthesizer.h"

#include <yarp/os/LogComponent.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Value.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <fstream>
#include <locale>
#include <sstream>
#include <map>


#if defined(__unix__) || defined(__APPLE__)
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

YARP_LOG_COMPONENT(KOKOROSPEECHSYNTHESIZER,
                  "yarp.device.kokoroSpeechSynthesizer",
                  yarp::os::Log::TraceType)

namespace {

const char* const kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const std::map<std::string, yarp::sig::VectorOfString> kokoroVoices = {
    {"a", yarp::sig::VectorOfString{
        "af_heart",
        "af_alloy",
        "af_aoede",
        "af_bella",
        "af_jessicas",
        "af_kore",
        "af_nicole",
        "af_nova",
        "af_river",
        "af_sarah",
        "af_sky",
        "am_adam",
        "am_echo",
        "am_eric",
        "am_fenrir",
        "am_liam",
        "am_michael",
        "am_onyx",
        "am_puck",
        "am_santa"
    }},
    {"b", yarp::sig::VectorOfString{
        "bf_alice",
        "bf_emma",
        "bf_isabella",
        "bf_lily",
        "bm_daniel",
        "bm_fable",
        "bm_george",
        "bm_lewis"
    }},
    {"j", yarp::sig::VectorOfString{
        "jf_alpha",
        "jf_gongitsune",
        "jf_nezumi",
        "jf_tebukuro",
        "jm_kumo"
    }},
    {"e", yarp::sig::VectorOfString{
        "ef_dora",
        "em_alex",
        "em_santa"
    }},
    {"z", yarp::sig::VectorOfString{
        "zf_xiaobei",
        "zf_xiaoni",
        "zf_xiaoxiao",
        "zf_xiaoyi",
        "zm_yunjian",
        "zm_yunxi",
        "zm_yunxia",
        "zm_yunyang"
    }},
    {"f", yarp::sig::VectorOfString{
        "ff_siwis"
    }},
    {"h", yarp::sig::VectorOfString{
        "hf_alpha",
        "hf_beta",
        "hm_omega",
        "hm_psi"
    }},
    {"i", yarp::sig::VectorOfString{
        "if_sara",
        "im_nicola"
    }},
    {"p", yarp::sig::VectorOfString{
        "pf_dora",
        "pm_alex",
        "pm_santa"
    }}
};

bool fileExists(const std::string& path)
{
    std::ifstream file(path);
    return file.good();
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

KokoroSpeechSynthesizer::~KokoroSpeechSynthesizer()
{
    close();
}

bool KokoroSpeechSynthesizer::open(yarp::os::Searchable& config)
{
    if (m_open) {
        return true;
    }

    if (!parseParams(config)) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Failed to parse device parameters";
        return false;
    }
    m_pythonExecutable = m_python_executable;
    m_workerScript = m_worker_script;
    m_modelRepository = m_model_repository;
    m_device = m_torch_device;
    m_voiceExplicit = config.check("voice") && m_voice != "auto";

    if (m_workerScript.empty()) {
#ifdef KOKORO_WORKER_SCRIPT_SOURCE
        if (fileExists(KOKORO_WORKER_SCRIPT_SOURCE)) {
            m_workerScript = KOKORO_WORKER_SCRIPT_SOURCE;
        }
#endif
    }
    if (m_workerScript.empty()) {
#ifdef KOKORO_WORKER_SCRIPT_INSTALL
        m_workerScript = KOKORO_WORKER_SCRIPT_INSTALL;
#endif
    }
    if (m_workerScript.empty() || !fileExists(m_workerScript)) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Kokoro worker script not found. Set the 'worker_script' parameter.";
        return false;
    }

    if (!setLanguage(m_language) || !setVoice(m_voice)
        || !setSpeed(m_speed) || !setPitch(m_pitch)) {
        return false;
    }

    if (!startWorker()) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Unable to start Kokoro worker";
        return false;
    }
    m_open = true;
    yCInfo(KOKOROSPEECHSYNTHESIZER) << "Kokoro speech synthesizer opened with language"
                                     << m_currentLanguage << "and voice" << m_currentVoice;
    return true;
}

bool KokoroSpeechSynthesizer::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_open
#if defined(__unix__) || defined(__APPLE__)
        && m_workerPid < 0
#endif
    ) {
        return true;
    }
    stopWorker();
    m_open = false;
    return true;
}

bool KokoroSpeechSynthesizer::startWorker()
{
#if defined(__unix__) || defined(__APPLE__)
    int parentToChild[2]{};
    int childToParent[2]{};
    if (pipe(parentToChild) != 0 || pipe(childToParent) != 0) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "pipe() failed:" << std::strerror(errno);
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "fork() failed:" << std::strerror(errno);
        ::close(parentToChild[0]);
        ::close(parentToChild[1]);
        ::close(childToParent[0]);
        ::close(childToParent[1]);
        return false;
    }

    if (pid == 0) {
        dup2(parentToChild[0], STDIN_FILENO);
        dup2(childToParent[1], STDOUT_FILENO);
        ::close(parentToChild[0]);
        ::close(parentToChild[1]);
        ::close(childToParent[0]);
        ::close(childToParent[1]);

        execlp(m_pythonExecutable.c_str(),
               m_pythonExecutable.c_str(),
               "-u",
               m_workerScript.c_str(),
               "--stdio",
               "--repo-id",
               m_modelRepository.empty() ? "hexgrad/Kokoro-82M" : m_modelRepository.c_str(),
               "--device",
               m_device.c_str(),
               static_cast<char*>(nullptr));
        _exit(127);
    }

    ::close(parentToChild[0]);
    ::close(childToParent[1]);
    m_workerInput = parentToChild[1];
    m_workerOutput = childToParent[0];
    m_workerPid = pid;

    std::string line;
    if (!readLine(line, 120000) || line != "READY") {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Kokoro worker did not become ready:" << line;
        stopWorker();
        return false;
    }
    return true;
#else
    yCError(KOKOROSPEECHSYNTHESIZER) << "The Kokoro device currently requires POSIX process support";
    return false;
#endif
}

void KokoroSpeechSynthesizer::stopWorker()
{
#if defined(__unix__) || defined(__APPLE__)
    if (m_workerInput >= 0) {
        ::close(m_workerInput);
        m_workerInput = -1;
    }
    if (m_workerOutput >= 0) {
        ::close(m_workerOutput);
        m_workerOutput = -1;
    }
    if (m_workerPid >= 0) {
        int status = 0;
        if (waitpid(m_workerPid, &status, WNOHANG) == 0) {
            kill(m_workerPid, SIGTERM);
            for (int i = 0; i < 20 && waitpid(m_workerPid, &status, WNOHANG) == 0; ++i) {
                usleep(10000);
            }
            if (waitpid(m_workerPid, &status, WNOHANG) == 0) {
                kill(m_workerPid, SIGKILL);
                waitpid(m_workerPid, &status, 0);
            }
        }
        m_workerPid = -1;
    }
    m_readBuffer.clear();
#endif
}

bool KokoroSpeechSynthesizer::writeAll(const std::string& data)
{
#if defined(__unix__) || defined(__APPLE__)
    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = write(m_workerInput, data.data() + offset, data.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
#else
    (void)data;
    return false;
#endif
}

bool KokoroSpeechSynthesizer::readLine(std::string& line, int timeoutMs)
{
#if defined(__unix__) || defined(__APPLE__)
    line.clear();
    char buffer[4096];
    while (true) {
        // Bytes after a newline belong to the next line: keep them for the
        // next call instead of dropping them.
        const auto newline = std::find(m_readBuffer.begin(), m_readBuffer.end(), '\n');
        if (newline != m_readBuffer.end()) {
            line.assign(m_readBuffer.begin(), newline);
            m_readBuffer.erase(m_readBuffer.begin(), newline + 1);
            return true;
        }
        struct pollfd descriptor{};
        descriptor.fd = m_workerOutput;
        descriptor.events = POLLIN;
        const int ready = poll(&descriptor, 1, timeoutMs);
        if (ready <= 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            return false;
        }
        const ssize_t count = read(m_workerOutput, buffer, sizeof(buffer));
        if (count <= 0) {
            return false;
        }
        m_readBuffer.append(buffer, static_cast<size_t>(count));
    }
#else
    (void)line;
    (void)timeoutMs;
    return false;
#endif
}

std::string KokoroSpeechSynthesizer::base64Encode(const std::string& input)
{
    std::string result;
    result.reserve(((input.size() + 2) / 3) * 4);
    for (size_t i = 0; i < input.size(); i += 3) {
        const uint32_t a = static_cast<unsigned char>(input[i]);
        const uint32_t b = i + 1 < input.size() ? static_cast<unsigned char>(input[i + 1]) : 0;
        const uint32_t c = i + 2 < input.size() ? static_cast<unsigned char>(input[i + 2]) : 0;
        const uint32_t value = (a << 16) | (b << 8) | c;
        result += kBase64Alphabet[(value >> 18) & 63];
        result += kBase64Alphabet[(value >> 12) & 63];
        result += i + 1 < input.size() ? kBase64Alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < input.size() ? kBase64Alphabet[value & 63] : '=';
    }
    return result;
}

bool KokoroSpeechSynthesizer::base64Decode(const std::string& input,
                                           std::vector<unsigned char>& output)
{
    output.clear();
    if (input.empty() || input.size() % 4 != 0) {
        return false;
    }
    auto valueOf = [](char c) -> int {
        const char* position = std::strchr(kBase64Alphabet, c);
        return position ? static_cast<int>(position - kBase64Alphabet) : -1;
    };
    output.reserve((input.size() / 4) * 3);
    for (size_t i = 0; i < input.size(); i += 4) {
        const int a = valueOf(input[i]);
        const int b = valueOf(input[i + 1]);
        const int c = input[i + 2] == '=' ? 0 : valueOf(input[i + 2]);
        const int d = input[i + 3] == '=' ? 0 : valueOf(input[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0 || (input[i + 2] == '=' && input[i + 3] != '=')) {
            return false;
        }
        const uint32_t value = (static_cast<uint32_t>(a) << 18)
                             | (static_cast<uint32_t>(b) << 12)
                             | (static_cast<uint32_t>(c) << 6)
                             | static_cast<uint32_t>(d);
        output.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
        if (input[i + 2] != '=') {
            output.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
        }
        if (input[i + 3] != '=') {
            output.push_back(static_cast<unsigned char>(value & 0xff));
        }
    }
    return true;
}

bool KokoroSpeechSynthesizer::workerSynthesize(const std::string& text,
                                               yarp::sig::Sound& sound)
{
#if defined(__unix__) || defined(__APPLE__)
    if (text.empty()) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Cannot synthesize empty text";
        return false;
    }
    // Language and voice are restricted to the Kokoro alphabet and are sent
    // as plain fields. Text is base64 encoded so arbitrary UTF-8 is safe.
    // The speed is formatted with the classic locale: std::to_string follows
    // the C locale and would emit "1,500000" under e.g. it_IT, which Python's
    // float() rejects.
    std::ostringstream speedText;
    speedText.imbue(std::locale::classic());
    speedText << m_currentSpeed;
    const std::string request = "SYNTH " + m_kokoroLanguage + " " + m_currentVoice + " "
                              + speedText.str() + " " + base64Encode(text) + "\n";
    if (!writeAll(request)) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Unable to write to Kokoro worker";
        return false;
    }

    std::string response;
    if (!readLine(response, 300000)) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Timed out waiting for Kokoro worker";
        return false;
    }
    std::istringstream stream(response);
    std::string status;
    stream >> status;
    if (status == "ERR") {
        std::string encodedError;
        stream >> encodedError;
        std::vector<unsigned char> errorBytes;
        base64Decode(encodedError, errorBytes);
        yCError(KOKOROSPEECHSYNTHESIZER) << "Kokoro synthesis failed:"
                                         << std::string(errorBytes.begin(), errorBytes.end());
        return false;
    }
    if (status != "OK") {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Malformed response from Kokoro worker";
        return false;
    }

    int sampleRate = 0;
    int channels = 0;
    std::string encodedAudio;
    stream >> sampleRate >> channels >> encodedAudio;
    if (!stream || sampleRate <= 0 || channels <= 0 || channels > 2) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Invalid audio metadata from Kokoro worker";
        return false;
    }
    std::vector<unsigned char> pcm;
    if (!base64Decode(encodedAudio, pcm) || pcm.empty() || pcm.size() % (2 * channels) != 0) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Invalid PCM data from Kokoro worker";
        return false;
    }

    const size_t frames = pcm.size() / (2 * static_cast<size_t>(channels));
    yarp::sig::Sound result(2);
    result.setFrequency(sampleRate);
    result.resize(frames, channels);
    for (size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const size_t offset = (frame * static_cast<size_t>(channels) + static_cast<size_t>(channel)) * 2;
            const auto sample = static_cast<int16_t>(static_cast<uint16_t>(pcm[offset])
                                                     | (static_cast<uint16_t>(pcm[offset + 1]) << 8));
            result.set(sample, frame, channel);
        }
    }
    sound = result;
    return true;
#else
    (void)text;
    (void)sound;
    return false;
#endif
}

bool KokoroSpeechSynthesizer::knownLanguage(const std::string& language,
                                            std::string& kokoroCode)
{
    const std::string value = lower(language);
    if (value == "auto" || value == "a" || value == "en" || value == "en-us" || value == "eng") {
        kokoroCode = "a";
    } else if (value == "b" || value == "en-gb") {
        kokoroCode = "b";
    } else if (value == "e" || value == "es" || value == "es-es") {
        kokoroCode = "e";
    } else if (value == "f" || value == "fr" || value == "fr-fr") {
        kokoroCode = "f";
    } else if (value == "h" || value == "hi" || value == "hi-in") {
        kokoroCode = "h";
    } else if (value == "i" || value == "it" || value == "it-it") {
        kokoroCode = "i";
    } else if (value == "j" || value == "ja" || value == "ja-jp") {
        kokoroCode = "j";
    } else if (value == "p" || value == "pt" || value == "pt-br") {
        kokoroCode = "p";
    } else if (value == "z" || value == "zh" || value == "zh-cn") {
        kokoroCode = "z";
    } else {
        return false;
    }
    return true;
}

std::string KokoroSpeechSynthesizer::defaultVoiceFor(const std::string& kokoroCode)
{
    if (kokoroCode == "b") return "bf_emma";
    if (kokoroCode == "e") return "ef_dora";
    if (kokoroCode == "f") return "ff_siwis";
    if (kokoroCode == "h") return "hf_alpha";
    if (kokoroCode == "i") return "if_sara";
    if (kokoroCode == "j") return "jf_alpha";
    if (kokoroCode == "p") return "pf_dora";
    if (kokoroCode == "z") return "zf_xiaobei";
    return "af_heart";
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::setLanguage(const std::string& language)
{
    std::string code;
    if (!knownLanguage(language, code)) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Unsupported Kokoro language:" << language;
        return yarp::dev::ReturnValue::return_code::return_value_error_generic;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentLanguage = language;
    m_kokoroLanguage = code;
    if (!m_voiceExplicit) {
        m_currentVoice = defaultVoiceFor(code);
    }
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::getLanguage(std::string& language)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    language = m_currentLanguage;
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::setVoice(const std::string& voice_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_voiceExplicit = voice_name != "auto";
    m_currentVoice = m_voiceExplicit ? voice_name : defaultVoiceFor(m_kokoroLanguage);
    if (m_currentVoice.empty()) {
        return yarp::dev::ReturnValue::return_code::return_value_error_generic;
    }
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::getVoice(std::string& voice_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    voice_name = m_currentVoice;
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::getAvailableVoices(yarp::sig::VectorOfString& voices)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = kokoroVoices.find(m_kokoroLanguage);
    if (it != kokoroVoices.end()) {
        voices = it->second;
        return yarp::dev::ReturnValue::return_code::return_value_ok;
    }
    return yarp::dev::ReturnValue::return_code::return_value_error_generic;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::setSpeed(const double speed)
{
    if (!std::isfinite(speed) || speed <= 0.0 || speed > 4.0) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Speed must be in the range (0, 4]";
        return yarp::dev::ReturnValue::return_code::return_value_error_generic;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentSpeed = speed;
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::getSpeed(double& speed)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    speed = m_currentSpeed;
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::setPitch(const double pitch)
{
    if (!std::isfinite(pitch)) {
        return yarp::dev::ReturnValue::return_code::return_value_error_generic;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    // Kokoro does not expose pitch as a synthesis parameter. Keep the value
    // for interface compatibility, but document that it has no audio effect.
    m_currentPitch = pitch;
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::getPitch(double& pitch)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    pitch = m_currentPitch;
    return yarp::dev::ReturnValue::return_code::return_value_ok;
}

yarp::dev::ReturnValue KokoroSpeechSynthesizer::synthesize(const std::string& text,
                                                           yarp::sig::Sound& sound)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_open) {
        yCError(KOKOROSPEECHSYNTHESIZER) << "Device is not open";
        return yarp::dev::ReturnValue::return_code::return_value_error_generic;
    }
    return workerSynthesize(text, sound)
        ? yarp::dev::ReturnValue::return_code::return_value_ok
        : yarp::dev::ReturnValue::return_code::return_value_error_generic;
}
