#include "xy_graphics.hpp"

#include <dmaKit.h>

namespace xy
{

    u64 toGsColor(const Color &color)
    {
        return GS_SETREG_RGBAQ(color.r, color.g, color.b, (color.a + 1) >> 1, 0x00);
    }

    XYGraphics::XYGraphics() : gs_(nullptr), width_(640), height_(448) {}

    XYGraphics::~XYGraphics()
    {
        shutdown();
    }

    bool XYGraphics::init(int width, int height)
    {
        width_ = width;
        height_ = height;

        dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                    1 << DMA_CHANNEL_GIF);
        dmaKit_chan_init(DMA_CHANNEL_GIF);

        gs_ = gsKit_init_global();
        if (gs_ == nullptr)
        {
            return false;
        }

        gs_->Mode = GS_MODE_NTSC;
        gs_->Interlace = GS_INTERLACED;
        gs_->Field = GS_FIELD;
        gs_->Width = width_;
        gs_->Height = height_;
        gs_->PSM = GS_PSM_CT32;
        gs_->PSMZ = 0;
        gs_->DoubleBuffering = GS_SETTING_ON;
        gs_->ZBuffering = GS_SETTING_OFF;

        gsKit_init_screen(gs_);
        gsKit_mode_switch(gs_, GS_ONESHOT);
        
        gs_->PrimAlphaEnable = GS_SETTING_ON;
        gs_->PrimAAEnable = GS_SETTING_ON;
        
        gsKit_set_primalpha(gs_, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
        
        // Disable Alpha Test and Z-Test properly via gsKit
        gsKit_set_test(gs_, GS_ATEST_OFF);
        gsKit_set_test(gs_, GS_ZTEST_OFF);

        return true;
    }

    void XYGraphics::shutdown()
    {
        gs_ = nullptr;
    }

    void XYGraphics::beginFrame(const Color &clearColor)
    {
        if (!gs_)
        {
            return;
        }
        gsKit_clear(gs_, toGsColor(clearColor));
    }

    void XYGraphics::endFrame()
    {
        if (!gs_)
        {
            return;
        }
        gsKit_queue_exec(gs_);
        gsKit_sync_flip(gs_);
    }

    void XYGraphics::drawTexture(XYTexture &texture, float x, float y)
    {
        drawTexture(texture, x, y, static_cast<float>(texture.width()), static_cast<float>(texture.height()));
    }

    void XYGraphics::drawTexture(XYTexture &texture, float x, float y, float width, float height, const Color &tint)
    {
        if (!gs_ || !texture.valid())
        {
            return;
        }

        GSTEXTURE *raw = texture.raw();
        gsKit_prim_sprite_texture(gs_, raw, x, y, 0.0f, 0.0f, x + width, y + height,
                                  static_cast<float>(texture.width()), static_cast<float>(texture.height()), 1,
                                  toGsColor(tint));
    }

    void XYGraphics::drawRect(float x, float y, float width, float height, const Color &color)
    {
        if (!gs_)
        {
            return;
        }
        gsKit_prim_sprite(gs_, x, y, x + width, y + height, 1, toGsColor(color));
    }

    int XYGraphics::width() const
    {
        return width_;
    }

    int XYGraphics::height() const
    {
        return height_;
    }

    GSGLOBAL *XYGraphics::gs()
    {
        return gs_;
    }

} // namespace xy
