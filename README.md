This repository supports Opus compression for 1980s / 90s sample-based music
formats (aka. "module" files / "tracker music" / etc).
The compression ratio depends on the size & number of the bundled
samples, which is often already small to reduce filesize. The Opus-compressed
files are 5-7x smaller on average, though it can get as high as 10-12x smaller
if a song makes heavy use of long 16-bit samples.

## Supported Formats
- `.mod` (Ultimate SoundTracker / ProTracker / etc., 1987)
- `.xm` (FastTracker II, 1994)
- `.s3m` (ScreamTracker 3, 1994)
- `.it` (Impulse Tracker 1 and 2, 1995 - 1999)

## "Why not use OggMod or MO3?"
As far as I can tell, OggMod never had its source code released, and was a
Windows-only GUI program. I also couldn't find the original source or author,
only some re-uploads from forum users almost 25 years ago. MO3 is much better,
but only does MP3 and Vorbis encoding. I wanted to use the best audio codec
available, which at the moment seems to be Opus.

## Technical Details
The library just places an Opus OGG file where the raw PCM sample is supposed to
be, and adjusts the metadata to match the new size. The compressed file will
still be readable, but have garbage audio until decompressed.

Since the sample rate is determined by the note being played, the samples are
always given to Opus as 48kHz (its native sample rate). For the rare stereo
samples in newer formats, they're still compressed as mono (since Opus expects
interleaved samples, and modules store de-interleaved samples).

Samples under 1KiB are left uncompressed, since the OGG container has an
overhead of 800-900 bytes. These are copied as-is by the decompressor once it
realizes the data isn't OGG.

### Integrating as a library
The main library (all the C files in `src` except `main.c`) relies on
`libopusenc` (which relies on `libopus`) and `libopusfile`. If you use CMake,
there will be a static library target named `moduleCompression` that compiles
everything for you. Your C compiler must support C11 (for `static_assert`).

### CLI Tool
`src/main.c` is a CLI tool that wraps the main library (via `--compress` and
`--decompress`) and acts as an audio player (when given just 1 argument).
The built-in audio player supports WAV, MP3, OGG (Vorbis or Opus), MOD, XM, S3M,
and IT. Opus-compressed modules can be played directly.

### Future Plans
I'd like to eventually store a raw Opus stream if possible, to save a few
hundred bytes per sample taken up by the OGG container.