# Xy - PS2 Game/Homebrew engine

C++ PS2 engine using ps2sdk, gsKit and audsrv.

## Layout

- `src/`: engine modules.
  - `xy_alloc`: custom EE/VRAM memory allocation.
  - `xy_game`: `XYGame` base class and main loop.
  - `xy_graphics`: gsKit init, frame, sprites and rectangles.
  - `xy_audio`: PCM mixer with voices (BGM + 8 SFX), supports WAV and SND loading.
  - `image/`: PNG/JPG/P2TX image loading and VRAM uploading.
  - `sound/`: WAV and SND format loaders with resource management.
  - `xy_input`: two joypads with pressed/down/released states.
  - `xy_debug_text`: 5x7 pixel font overlay.
- `tools/`: offline asset converters.
- `docs/`: format specs and usage guides.

## Docs

- [App / Game Interface](docs/app.md) — `XYGame` lifecycle and core services.
- [Input System](docs/input.md) — joypad state, buttons, and examples.
- [Task System](docs/tasks.md) — main-thread timers and EE async threads.
- [Font Runtime](docs/fonts.md) — loading and rendering custom fonts.
- [Audio System](docs/audio.md) — P2SN format, converter, mixer API.
- [Image System](docs/image.md) — P2TX format, converter, texture API.
- [Font Format](docs/font_format.md) — `.p2f` binary format and converter details.

## Asset Pipeline

Source assets (WAV, PNG, JPG) are converted offline to PS2-optimized binary formats:

```
.wav  → tools/ps2snd.py → .snd    (P2SN: 64-byte header + aligned PCM16)
.png  → tools/ps2tex.py → .ps2tex (P2TX: 40-byte header + GS pixel data + CLUT)
.jpg  → tools/ps2tex.py → .ps2tex
```

## Examples

- `examples/audio`: looping BGM plus CROSS/CIRCLE/SQUARE one-shot SFX (using `.snd`).
- `examples/debug_text`: debug overlay and glyph sample.
- `examples/input_status`: text status for both joypads.
- `examples/render_images`: JPG background plus centered PNG sprite (using `.ps2tex`).
- `examples/custom_font`: Demonstrates custom BMFont loading and rendering.
- `examples/tasks`: Demonstrates delayed and repeated tasks.

## Tools

- `tools/ps2snd.py`: WAV → `.snd` (P2SN) converter. Supports `--mono`, `--resample`, `--loop-start/end`.
- `tools/ps2tex.py`: PNG/JPG → `.ps2tex` (P2TX) converter. Supports `--format` (psmt8/psmt4/ct16/ct32) and `--swizzle`.
- `Font Tool`: Use any BMFont-compatible exporter (like AngelCode BMFont or Littera) to generate `.fnt` files.

## Build

```sh
# Windows:
build.bat render_images
build.bat audio
build.bat input_status
build.bat debug_text
build.bat tasks
build.bat all

# Linux/MacOS
sh build.sh render_images
sh build.sh audio
sh build.sh input_status
sh build.sh debug_text
sh build.sh tasks
sh build.sh all
```

You can also build every example after the Docker image exists:

```sh
docker run --rm -v "$(pwd):/xy" -w /xy xy-ps2 make all
```

When running an ELF through `host:`, run it with the example folder as host root so paths like `host:assets/sprite.ps2tex` and `host:audsrv.irx` resolve.
