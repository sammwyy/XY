# Xyon PS2 Engine

Small C++ PS2 engine scaffold using ps2sdk, gsKit and audsrv.

## Layout

- `src/`: engine modules.
  - `xy_game`: `XYGame` base class and loop.
  - `xy_image`: PNG/JPG texture wrapper.
  - `xy_graphics`: gsKit init, frame, sprites and rectangles.
  - `xy_input`: two joypads with pressed/down/released states.
  - `xy_audio`: WAV PCM mixer with looping BGM and one-shot SFX.
  - `xy_debug_text`: 5x7 pixel font overlay.
- `examples/render_images`: JPG background plus centered PNG sprite.
- `examples/audio`: looping BGM plus CROSS/CIRCLE/SQUARE one-shots.
- `examples/input_status`: text status for both joypads.
- `examples/debug_text`: debug overlay and glyph sample.

## Build

Windows:

```bat
build.bat render_images
build.bat audio
build.bat input_status
build.bat debug_text
build.bat all
```

Linux/macOS:

```sh
sh build.sh render_images
sh build.sh audio
sh build.sh input_status
sh build.sh debug_text
sh build.sh all
```

You can also build every example after the Docker image exists:

```sh
docker run --rm -v "$(pwd):/xyon" -w /xyon xyon-ps2 make all
```

When running an ELF through `host:`, run it with the example folder as host root so paths like `host:assets/sprite.png` and `host:audsrv.irx` resolve.

## Images

`XYTexture::load` detects `.png`, `.jpg` and `.jpeg` automatically:

```cpp
texture.load(graphics().gs(), "host:assets/sprite.png");
```

Manual format selection is available when the path has no useful extension:

```cpp
texture.load(graphics().gs(), "host:assets/background", xy::XY_IMAGE_JPG);
texture.loadPng(graphics().gs(), "host:assets/sprite.bin");
texture.loadJpg(graphics().gs(), "host:assets/background.bin");
```

For manual VRAM upload:

```cpp
texture.load(graphics().gs(), "host:assets/sprite.png", xy::XY_IMAGE_AUTO, xy::XY_TEXTURE_UPLOAD_MANUAL);
texture.upload(graphics().gs());
```
