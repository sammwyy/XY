"""
PS2 Sound Converter for Xyon Engine
Converts WAV files to .snd (P2SN) format for PS2 runtime.

Usage:
  python ps2snd.py input.wav output.snd
  python ps2snd.py input.wav output.snd --loop-start 10000 --loop-end 250000
  python ps2snd.py input.wav output.snd --resample 22050
  python ps2snd.py input.wav output.snd --mono

Format:
  [Ps2SndHeader]  64 bytes
  [PCM16 data]    64-byte aligned
"""

import wave
import struct
import argparse
from pathlib import Path

# --- Constants ---

MAGIC = b"P2SN"
VERSION = 1
HEADER_SIZE = 64

# Codecs
CODEC_PCM16 = 1

# Flags
FLAG_LOOP     = 1 << 0
FLAG_STEREO   = 1 << 1
FLAG_STREAMED = 1 << 2


def align(data: bytes, alignment: int) -> bytes:
    """Pad data to the specified alignment."""
    padding = (-len(data)) % alignment
    return data + bytes(padding)


def stereo_to_mono(raw: bytes, sample_count: int) -> bytes:
    """Mix stereo PCM16 LE to mono by averaging L+R."""
    out = bytearray(sample_count * 2)
    for i in range(sample_count):
        off = i * 4
        left  = struct.unpack_from("<h", raw, off)[0]
        right = struct.unpack_from("<h", raw, off + 2)[0]
        mono  = (left + right) // 2
        struct.pack_into("<h", out, i * 2, mono)
    return bytes(out)


def resample_pcm16(raw: bytes, channels: int,
                   src_rate: int, dst_rate: int) -> bytes:
    """Nearest-neighbour resample for PCM16 LE data."""
    frame_size = channels * 2  # bytes per frame
    src_frames = len(raw) // frame_size
    dst_frames = int(src_frames * dst_rate / src_rate)

    out = bytearray(dst_frames * frame_size)
    for i in range(dst_frames):
        src_i = int(i * src_rate / dst_rate)
        if src_i >= src_frames:
            src_i = src_frames - 1
        src_off = src_i * frame_size
        dst_off = i * frame_size
        out[dst_off:dst_off + frame_size] = raw[src_off:src_off + frame_size]
    return bytes(out)


def convert_wav_to_ps2snd(input_path, output_path,
                          loop_start=0, loop_end=0,
                          force_mono=False, target_rate=0):
    """Convert a WAV file to PS2 .snd (P2SN) format."""

    with wave.open(str(input_path), "rb") as wav:
        channels    = wav.getnchannels()
        sample_rate = wav.getframerate()
        sample_width = wav.getsampwidth()
        num_frames  = wav.getnframes()
        raw         = wav.readframes(num_frames)

    if channels not in (1, 2):
        raise ValueError(f"Only mono/stereo WAV supported (got {channels} channels)")

    if sample_width != 2:
        raise ValueError(f"Only 16-bit PCM WAV supported (got {sample_width * 8}-bit)")

    # --- Optional mono downmix ---
    if force_mono and channels == 2:
        raw = stereo_to_mono(raw, num_frames)
        channels = 1

    # --- Optional resample ---
    if target_rate and target_rate != sample_rate:
        old_frames = num_frames
        raw = resample_pcm16(raw, channels, sample_rate, target_rate)
        num_frames = len(raw) // (channels * 2)
        # Adjust loop points proportionally
        if loop_end > loop_start:
            ratio = target_rate / sample_rate
            loop_start = int(loop_start * ratio)
            loop_end   = int(loop_end * ratio)
        sample_rate = target_rate

    # --- Compute per-channel sample count ---
    sample_count = num_frames  # frames = samples per channel

    # --- Flags ---
    flags = 0
    if channels == 2:
        flags |= FLAG_STEREO

    if loop_end > loop_start:
        flags |= FLAG_LOOP
    else:
        loop_start = 0
        loop_end = 0

    # --- Align audio data to 64 bytes ---
    data = align(raw, 64)
    data_size = len(data)
    data_offset = HEADER_SIZE

    # --- Build header (64 bytes) ---
    # 4+2+2+4+4+4+4+4+4+4+1+27 = 64
    header = struct.pack(
        "<4s HH I I I I I I I B 27s",
        MAGIC,
        VERSION,
        channels,
        sample_rate,
        sample_count,
        loop_start,
        loop_end,
        data_size,
        data_offset,
        flags,
        CODEC_PCM16,
        bytes(27),
    )
    assert len(header) == HEADER_SIZE, f"Header is {len(header)} bytes, expected {HEADER_SIZE}"

    # --- Write ---
    with open(output_path, "wb") as f:
        f.write(header)
        f.write(data)

    total_bytes = HEADER_SIZE + data_size
    duration_s = sample_count / sample_rate if sample_rate else 0

    print(f"Success: {input_path} -> {output_path}")
    print(f"  Channels:    {channels}")
    print(f"  Sample rate: {sample_rate} Hz")
    print(f"  Samples:     {sample_count}")
    print(f"  Duration:    {duration_s:.2f}s")
    print(f"  Codec:       PCM16")
    print(f"  Flags:       0x{flags:02X}")
    print(f"  Data size:   {data_size} bytes")
    print(f"  Total file:  {total_bytes} bytes")

    if flags & FLAG_LOOP:
        print(f"  Loop:        {loop_start} -> {loop_end}")


def main():
    parser = argparse.ArgumentParser(
        description="PS2 Sound Converter for Xyon Engine — WAV to .snd (P2SN)")
    parser.add_argument("input", help="Input .wav file")
    parser.add_argument("output", help="Output .snd file")
    parser.add_argument("--loop-start", type=int, default=0,
                        help="Loop start sample (per channel)")
    parser.add_argument("--loop-end", type=int, default=0,
                        help="Loop end sample (per channel, 0 = no loop)")
    parser.add_argument("--mono", action="store_true",
                        help="Force downmix to mono")
    parser.add_argument("--resample", type=int, default=0,
                        metavar="RATE",
                        help="Resample to target rate (e.g. 22050)")

    args = parser.parse_args()

    convert_wav_to_ps2snd(
        Path(args.input),
        Path(args.output),
        loop_start=args.loop_start,
        loop_end=args.loop_end,
        force_mono=args.mono,
        target_rate=args.resample,
    )


if __name__ == "__main__":
    main()
