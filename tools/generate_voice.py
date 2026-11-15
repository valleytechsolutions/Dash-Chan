"""Synthesize the spoken clips listed in tools/voice_phrases.tsv into assets/voice (Linux).

Build-time only: none of these tools ship in the firmware. See docs/licenses/VOICE-NOTICE.txt.

  pip install piper-tts faster-whisper      # faster-whisper is only needed for the --asr options
  apt-get install ffmpeg                    # must include the rubberband and soxr filters
  mkdir -p .tools/voice && cd .tools/voice && python -m piper.download_voices en_US-ljspeech-high

  python tools/generate_voice.py --asr                 every synthesized clip; keep only takes that two
                                                       speech recognisers both read back word for word
  python tools/generate_voice.py --asr --only flock    just this one
  python tools/generate_voice.py --check-asr           transcribe the committed clips; changes nothing
  python tools/generate_voice.py --legacy-header old_voice.h --only none
        re-encode the three older recordings from the include/voice.h of release 0.5.0 or earlier

Every run of the engine sounds slightly different, so commit assets/voice/*.wav and the regenerated
src/voice_data.cpp together. A recogniser reading a clip back is a sanity check, not a listening test.
"""
from pathlib import Path
import argparse, difflib, hashlib, os, re, subprocess, sys, tempfile, wave

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_voice
from pack_voice import CLIPS, MAX_SECONDS, RATE, manifest, root

MODEL_NAME = 'en_US-ljspeech-high'
MODEL_SHA256 = '5d4f08ba6a2a48c44592eed3ce56bf85e9de3dd4e20df90541ae68a8310c029a'
# Leading and trailing silence costs flash and delays the alert. The threshold is low on purpose:
# at -45 dB the trim also ate soft first consonants ("That tickles" came back as "at tickles").
TRIM = ('silenceremove=start_periods=1:start_threshold=-60dB:start_silence=0.03,areverse,'
        'silenceremove=start_periods=1:start_threshold=-60dB:start_silence=0.05,areverse')
# Small-speaker voicing: drop rumble the driver cannot play, lift consonants, even out the level,
# and fade both ends so 8-bit playback does not click.
POST = ('highpass=f=180,treble=g=3:f=3500,acompressor=threshold=-18dB:ratio=3:attack=5:release=60:makeup=4,'
        'alimiter=limit=0.89,afade=t=in:d=0.008,areverse,afade=t=in:d=0.02,areverse')
# Five semitones up in total. Two move the formants too (a smaller creature); the other three keep
# them, which is what stays intelligible. The engine's reading pace is kept: speeding it up by
# 8 % measurably cost recognition, and the clips are short enough already.
FORMANT_SHIFT = 2 ** (2 / 12)
PITCH_ONLY = 2 ** (3 / 12)
TEMPO = 1.0
CUTE = (f'aresample=22050,asetrate=22050*{FORMANT_SHIFT:.5f},aresample=22050,'
        f'rubberband=pitch={PITCH_ONLY:.5f}:tempo={TEMPO / FORMANT_SHIFT:.5f}:formant=preserved')
TO_8BIT = f'aresample={RATE}:resampler=soxr:dither_method=triangular'
LEGACY_ARRAYS = {'voicePig': 'spicy_pig', 'voiceFuzz': 'spicy_fuzz', 'voiceLove': 'love'}


def encode(source, target, filters):
    """Run one ffmpeg filter chain and write the canonical 8-bit WAV the firmware expects."""
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(source), '-af', ','.join(filters + [TO_8BIT]),
                    '-map_metadata', '-1', '-fflags', '+bitexact', '-flags:a', '+bitexact', '-ac', '1',
                    '-c:a', 'pcm_u8', str(target)], check=True)
    # ffmpeg pads an odd-length data chunk. Keep an even count instead, so the file is exactly
    # the 44-byte header plus samples.
    data = Path(target).read_bytes()
    assert data[36:40] == b'data', 'unexpected WAV layout from ffmpeg'
    samples = data[44:44 + int.from_bytes(data[40:44], 'little')]
    samples = samples[:len(samples) & ~1]
    Path(target).write_bytes(pack_voice.wav_header(len(samples)) + samples)


def seconds(path):
    return (path.stat().st_size - pack_voice.HEADER) / RATE


def norm(text):
    return re.sub(r'[^a-z0-9]', '', text.lower())


class Recogniser:
    """Speech-to-text used to reject garbled takes. Optional: only loaded for the --asr options."""

    def __init__(self, name):
        from faster_whisper import WhisperModel
        self.name = name
        self.model = WhisperModel(name, device='cpu', compute_type='int8')

    def hear(self, path):
        import numpy
        raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-f', 's16le', '-ac', '1', '-ar', '16000',
                              '-'], capture_output=True, check=True).stdout
        audio = numpy.frombuffer(raw, numpy.int16).astype(numpy.float32) / 32768
        return ' '.join(s.text.strip() for s in self.model.transcribe(audio, beam_size=5)[0])


def understood(row, heard):
    """Word for word, ignoring case, spacing and punctuation. The caption counts too, and so do the
    manifest's "heard as" spellings: other ways to write exactly the same sounds."""
    return norm(heard) in {norm(text) for text in [row['spoken'], row['caption']] + row['heard']}


def synthesize(rows, args, work):
    from piper import PiperVoice
    model = Path(args.model)
    if not model.is_file():
        sys.exit(f'Voice model not found at {model}. See the setup notes at the top of this file.')
    digest = hashlib.sha256(model.read_bytes()).hexdigest()
    if digest != MODEL_SHA256:
        print(f'WARNING: {model.name} is not the recorded model (sha256 {digest}); update the voice notice.')
    voice = PiperVoice.load(str(model))
    ears = [Recogniser(name) for name in args.asr_models.split(',')] if args.asr else []
    failed = []
    for row in rows:
        best = None  # (score, take path, transcripts)
        for take in range(args.takes if ears else 1):
            raw, cooked = work / f"{row['id']}-{take}.raw.wav", work / f"{row['id']}-{take}.wav"
            with wave.open(str(raw), 'wb') as out:
                voice.synthesize_wav(row['spoken'], out)
            encode(raw, cooked, [TRIM, CUTE, POST])
            fits = seconds(cooked) <= MAX_SECONDS
            heard = [ear.hear(cooked) for ear in ears]
            agree = sum(understood(row, h) for h in heard)
            ok = fits and agree == len(ears)
            close = sum(difflib.SequenceMatcher(None, norm(h), norm(row['spoken'])).ratio() for h in heard)
            score = (ok, fits, agree, close)
            if not best or score > best[0]:
                best = (score, cooked, heard)
            if ok:
                break
        score, cooked, heard = best
        (CLIPS / f"{row['id']}.wav").write_bytes(cooked.read_bytes())
        note = '' if score[0] else '  <-- too long' if not score[1] else '  <-- not read back by every recogniser'
        print(f"{row['id']:<13} {seconds(cooked):.2f} s  {' | '.join(map(repr, heard))}{note}")
        if not score[0]:
            failed.append(row['id'])
    return failed


def reencode_legacy(rows, header, work):
    """The older clips came from a Windows speech engine and exist only as arrays in an old header."""
    text = Path(header).read_text()
    for name, clip in LEGACY_ARRAYS.items():
        if not any(row['id'] == clip for row in rows):
            continue
        found = re.search(r'static const uint8_t ' + name + r'\[\] = \{([^}]*)\}', text)
        assert found, f'{name} not found in {header}'
        source = work / f'{clip}.orig.wav'
        source.write_bytes(bytes(int(v) for v in found.group(1).replace('\n', '').split(',') if v.strip()))
        encode(source, CLIPS / f'{clip}.wav', [TRIM, 'afade=t=in:d=0.008,areverse,afade=t=in:d=0.02,areverse'])
        print(f'{clip:<13} {seconds(CLIPS / (clip + ".wav")):.2f} s  re-encoded from {name}')


def check_asr(rows, names):
    ears = [Recogniser(name) for name in names.split(',')]
    passed = [0] * len(ears)
    every = 0
    for row in rows:
        path = CLIPS / f"{row['id']}.wav"
        heard = [ear.hear(path) for ear in ears]
        ok = [understood(row, h) for h in heard]
        passed = [p + o for p, o in zip(passed, ok)]
        every += all(ok)
        print(f"{'ok  ' if all(ok) else 'MISS'} {row['id']:<13} {seconds(path):.2f} s  "
              f"{' | '.join(map(repr, heard))}")
    for ear, count in zip(ears, passed):
        print(f'{ear.name}: {count}/{len(rows)} clips transcribed word for word')
    print(f'every recogniser: {every}/{len(rows)}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--model', default=os.environ.get('DASH_VOICE_MODEL',
                                                          str(root / '.tools' / 'voice' / f'{MODEL_NAME}.onnx')))
    parser.add_argument('--only', help='comma-separated clip ids (default: every synthesized clip)')
    parser.add_argument('--asr', action='store_true', help='retry until every recogniser reads the take back')
    parser.add_argument('--takes', type=int, default=12, help='attempts per clip with --asr')
    parser.add_argument('--asr-models', default='small.en,base.en', help='faster-whisper models, comma-separated')
    parser.add_argument('--check-asr', action='store_true', help='only transcribe the committed clips')
    parser.add_argument('--legacy-header', help='old include/voice.h holding voicePig, voiceFuzz, voiceLove')
    args = parser.parse_args()
    rows = manifest()
    wanted = set(args.only.split(',')) if args.only else None
    unknown = (wanted or set()) - {row['id'] for row in rows} - {'none'}
    if unknown:
        sys.exit(f'Unknown clip ids: {sorted(unknown)}')
    chosen = [row for row in rows if not wanted or row['id'] in wanted]
    if args.check_asr:
        check_asr(chosen, args.asr_models)
        sys.exit(0)
    CLIPS.mkdir(parents=True, exist_ok=True)
    failed = []
    with tempfile.TemporaryDirectory() as work:
        if args.legacy_header:
            reencode_legacy([row for row in rows if row['legacy']], args.legacy_header, Path(work))
        todo = [row for row in chosen if not row['legacy']]
        if todo:
            failed = synthesize(todo, args, Path(work))
    if all((CLIPS / f"{row['id']}.wav").is_file() for row in rows):
        subprocess.run([sys.executable, str(root / 'tools' / 'pack_voice.py')], check=True)
    else:
        print('Some clips are still missing, so src/voice_data.cpp was not rebuilt.')
    if failed:
        sys.exit(f'Reword or respell these in tools/voice_phrases.tsv: {", ".join(failed)}')
