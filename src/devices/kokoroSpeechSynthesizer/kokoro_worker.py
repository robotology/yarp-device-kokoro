#!/usr/bin/env python3
"""Persistent stdio worker used by yarp_kokoroSpeechSynthesizer.

The protocol deliberately has no JSON dependency: all strings are base64
encoded and each request/response is a single line.

Request::
    SYNTH <language-code> <voice> <speed> <base64-utf8-text>

Response::
    OK <sample-rate> <channels> <base64-little-endian-s16>
    ERR <base64-utf8-error>
"""

from __future__ import annotations

import argparse
import base64
import sys
from typing import Dict


def decode(value: str) -> str:
    return base64.b64decode(value.encode("ascii"), validate=True).decode("utf-8")


def encode(value: str) -> str:
    return base64.b64encode(value.encode("utf-8")).decode("ascii")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stdio", action="store_true")
    parser.add_argument("--repo-id", default="hexgrad/Kokoro-82M")
    parser.add_argument("--device", default="")
    args = parser.parse_args()
    if not args.stdio:
        parser.error("only --stdio mode is supported")

    # Import failures are reported as an ERR line in place of READY, so the
    # device fails at open() time. Model download/loading is deferred to the
    # first synthesis for each language, which reports errors as a normal ERR
    # response.
    try:
        import numpy as np
        from kokoro import KPipeline
    except Exception as exc:  # pragma: no cover - depends on the user's env
        print(f"ERR {encode('Unable to import Kokoro dependencies: ' + str(exc))}", flush=True)
        return 1

    pipelines: Dict[str, object] = {}

    print("READY", flush=True)
    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        try:
            fields = line.split(" ", 4)
            if len(fields) != 5 or fields[0] != "SYNTH":
                raise ValueError("malformed SYNTH request")
            language, voice, speed_text, text_b64 = fields[1:]
            text = decode(text_b64)
            speed = float(speed_text)
            if not text.strip():
                raise ValueError("text must not be empty")
            if speed <= 0:
                raise ValueError("speed must be positive")

            if language not in pipelines:
                pipelines[language] = KPipeline(
                    lang_code=language,
                    repo_id=args.repo_id,
                    device=(args.device or None),
                )
            pipeline = pipelines[language]

            chunks = []
            sample_rate = 24000
            for result in pipeline(text, voice=voice, speed=speed):
                audio = result.audio
                if audio is None:
                    continue
                array = audio.detach().to("cpu").numpy().astype(np.float32, copy=False)
                if array.size:
                    chunks.append(array.reshape(-1))
                    sample_rate = int(getattr(pipeline.model, "sample_rate", sample_rate))

            if not chunks:
                raise RuntimeError("Kokoro returned no audio")
            audio = np.concatenate(chunks)
            audio = np.clip(audio, -1.0, 1.0)
            pcm = (audio * 32767.0).astype("<i2", copy=False).tobytes()
            encoded = base64.b64encode(pcm).decode("ascii")
            print(f"OK {sample_rate} 1 {encoded}", flush=True)
        except Exception as exc:  # keep the worker alive for later requests
            print(f"ERR {encode(str(exc))}", flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
