# yarp-device-kokoro

A YARP device plugin exposing `yarp::dev::ISpeechSynthesizer` and generating
speech with [Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M).

## Design

The C++ plugin starts `kokoro_worker.py` as a persistent child process. Audio
requests are sent over a small line protocol on stdin/stdout, so the Python
Kokoro model remains loaded and Python does not need to be embedded in the
YARP process. The worker returns mono, 16-bit PCM which is converted to a
`yarp::sig::Sound` at Kokoro's sample rate.

The worker supports multiple requests and caches one `KPipeline` per language.
The first synthesis for a language downloads/loads the model if it is not
already in the Hugging Face cache.

## Dependencies

- YARP 4.x with `os`, `sig`, and `dev` components
- Python 3.10+ and the [`kokoro`](https://pypi.org/project/kokoro/) package
- The Python dependencies used by Kokoro (`torch`, `numpy`, etc.)
- Network access on the first run unless the model is already cached

For example, in the Python environment used by this machine:

```bash
python -m pip install kokoro
```

## Build

```bash
cmake -S . -B build -DYARP_DIR=/path/to/yarp/build
cmake --build build -j
```

Install the plugin in the normal YARP plugin path if desired:

```bash
cmake --install build --prefix /path/to/yarp/prefix
```

The build tree's worker script is used automatically. For an installed plugin,
the installed script is used automatically; `worker_script` can also be set
explicitly.

## Run with `yarpdev`

Start a YARP server, then run:

```bash
yarpdev --device kokoroSpeechSynthesizer \
  --python_executable /path/to/python \
  --language en \
  --voice af_heart \
  --speed 1.0
```

Useful configuration parameters:

| Parameter | Default | Meaning |
|---|---:|---|
| `python_executable` | `python3` | Python interpreter containing `kokoro` |
| `worker_script` | built/installed worker | Path to `kokoro_worker.py` |
| `model_repository` | `hexgrad/Kokoro-82M` | Hugging Face model repository |
| `torch_device` | automatic | PyTorch device, e.g. `cuda` or `cpu` (the YARP `device` key is reserved) |
| `language` | `en` | `en`, `en-gb`, `es`, `fr`, `hi`, `it`, `ja`, `pt-br`, `zh`, or Kokoro code |
| `voice` | language default | Kokoro voice, e.g. `af_heart` or `bf_emma` |
| `speed` | `1.0` | Positive Kokoro speed in `(0, 4]` |
| `pitch` | `0.0` | Stored for interface compatibility; Kokoro currently does not apply pitch |

The plugin currently uses POSIX pipes/processes (Linux and macOS). On Windows,
run the Python worker separately and adapt the transport before using the
plugin.

## Minimal C++ client example

```cpp
yarp::dev::PolyDriver driver({{"device", "kokoroSpeechSynthesizer"}});
yarp::dev::ISpeechSynthesizer* synthesizer = nullptr;
driver.view(synthesizer);
yarp::sig::Sound sound;
synthesizer->synthesize("Hello from YARP and Kokoro.", sound);
```

## Repository history

This directory is an independent Git repository. Commits are intentionally
small so implementation and validation steps can be inspected with `git log`.
