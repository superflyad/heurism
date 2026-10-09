"""User-session speaker playback/readback proof; silent PCM, no microphone recording."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import wave


def client():
    assert os.getuid() == 1000
    def pactl(*args):
        return subprocess.check_output(['pactl', *args], text=True, timeout=3).strip()
    sink = pactl('get-default-sink')
    assert 'Speaker' in sink
    descriptor, path = tempfile.mkstemp(suffix='.wav', prefix='companion-silent-')
    os.close(descriptor)
    process = None
    try:
        with wave.open(path, 'wb') as stream:
            stream.setnchannels(2)
            stream.setsampwidth(2)
            stream.setframerate(48000)
            stream.writeframes(b'\0'*48000*2*2)
        process = subprocess.Popen(['paplay', '--device='+sink, path], stderr=subprocess.PIPE, text=True)
        running = False
        for _ in range(15):
            lines = pactl('list', 'short', 'sinks').splitlines()
            if any(sink in line and line.endswith('RUNNING') for line in lines):
                running = True
                break
            time.sleep(0.05)
        error = process.communicate(timeout=5)[1]
        assert process.returncode == 0, error
        assert running, 'Speaker sink did not run during playback'
        print(json.dumps({'ok': True, 'uid': 1000, 'speaker_sink': sink,
                          'real_sink_running_during_pcm_playback': True,
                          'audible_sound_observed': False}))
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=3)
        Path(path).unlink(missing_ok=True)


if __name__ == '__main__':
    if '--client' in sys.argv:
        client()
    else:
        result = subprocess.check_output(['su', '-s', '/bin/sh', '-c',
            'XDG_RUNTIME_DIR=/run/companion-desktop/user python3 '+str(Path(__file__).resolve())+' --client',
            'companion-ui'], text=True, timeout=15)
        Path('/var/lib/companion/desktop-stage/evidence/audio-playback.json').write_text(result)
        print(result, end='')
