# Font Runtime

This page covers the runtime font API used by games. For the binary `.p2f` format and converter details, see `docs/font_format.md`.

Xy has two text paths:

1. **Debug text**: built into `XYGraphics::drawText`, used when no font handle is passed.
2. **Custom fonts**: loaded through `XYFont` and rendered by `XYGraphics`.

## Debug Text

The debug font is always available after graphics initialization.

```cpp
graphics().drawText(40, 40, "HELLO WORLD", xy::Color(255, 255, 255), 2.0f);
graphics().drawFormat(40, 80, xy::Color(255, 255, 90), 2.0f, "SCORE: %d", score);
```

Use this for diagnostics, quick prototypes, and examples that do not need an external font asset.

## Custom Font Lifecycle

```cpp
#include "font/xy_font.h"

class MyGame : public xy::XYGame {
public:
    bool onInit() override {
        xy::XYFont::init();

        if (!xy::XYFont::load("host:assets/ui.ps2fnt", &font_)) {
            return false;
        }

        return xy::XYFont::loadTextures(font_, graphics().gs());
    }

    void onShutdown() override {
        xy::XYFont::unloadTextures(font_, graphics().gs());
        xy::XYFont::unload(font_);
        xy::XYFont::shutdown();
    }

    void onRender() override {
        graphics().drawText(50, 50, "Custom font", xy::Color(255, 255, 255), 1.0f, font_);
    }

private:
    xy::XYFontHandle font_ = XY_FONT_INVALID_HANDLE;
};
```

`XYFont::load` loads font metadata. `XYFont::loadTextures` uploads the font pages to GS VRAM.

## Drawing Text

```cpp
graphics().drawText(x, y, "Text", xy::Color(255, 255, 255), 1.0f, font);

graphics().drawFormat(x, y, xy::Color(255, 255, 90), 1.0f, font,
                      "HP: %d/%d", hp, maxHp);
```

If the font handle is `XY_FONT_INVALID_HANDLE`, `drawText` uses the built-in debug glyphs.

## Measuring Text

```cpp
float width = xy::XYFont::measureTextWidth(font, "START", 1.0f);

float w = 0.0f;
float h = 0.0f;
xy::XYFont::measureText(font, "OPTIONS", 1.0f, &w, &h);
```

Use measuring for centered labels, menus, and HUD layout.

## Metrics

```cpp
xy::XYFontMetrics metrics = xy::XYFont::getMetrics(font);

int lineHeight = metrics.line_height;
int base = metrics.base;
```

Metrics come from the font data and scale with the `scale` value used during drawing.

## Manual Layout

Advanced renderers can generate vertices directly.

```cpp
xy::XYTextVertex vertices[512];
xy::XYTextLayoutParams params = {};
params.x = 100.0f;
params.y = 100.0f;
params.scale = 1.0f;
params.color = xy::toGsColor(xy::Color(255, 255, 255));

int count = xy::XYFont::layoutText(font, "Score", params, vertices, 512);
```

`layoutText` returns the number of vertices written. Each glyph emits 6 vertices.

## Texture Pages

```cpp
int pages = xy::XYFont::getPageCount(font);
const char* pagePath = xy::XYFont::getPagePath(font, 0);
GSTEXTURE* pageTexture = xy::XYFont::getPageTexture(font, 0);
```

Font textures are owned by the font runtime after `loadTextures`.

## Example

See `examples/custom_font`.

The example loads `opensans.ps2fnt`, uploads its texture pages, renders custom text, and compares it with the built-in debug font.

## Guidelines

| Use case | Recommendation |
|---|---|
| Debug counters | Built-in debug text |
| HUD and menus | Custom `.ps2fnt` / `.p2f` font |
| Large body text | Custom font with measured layout |
| Production assets | Convert offline with `tools/ps2fnt.py` |

Unload font textures before shutting down graphics.
