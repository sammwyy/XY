#include "xy_image.hpp"
#include "../xy_alloc.hpp"
#include <gsToolkit.h>
#include <malloc.h>
#include <cstring>
#include <dmaKit.h>
#include <kernel.h>

namespace xy {

XYImageJPG::XYImageJPG() {
    std::memset(&texture_, 0, sizeof(texture_));
    texture_.Delayed = 1;
}

XYImageJPG::~XYImageJPG() {
}

bool XYImageJPG::loadEE(const std::string& path) {
    path_ = path;
    return true;
}

namespace {
    inline int getBpp(int psm) {
        switch (psm) {
            case GS_PSM_CT32: return 32;
            case GS_PSM_CT24: return 24;
            case GS_PSM_CT16: case GS_PSM_CT16S: return 16;
            case GS_PSM_T8: return 8;
            case GS_PSM_T4: return 4;
            default: return 32;
        }
    }
}

bool XYImageJPG::loadGS(GSGLOBAL* gs) {
    if (texture_.Vram != 0) return true;

    int res = gsKit_texture_jpeg_scale(gs, &texture_, const_cast<char*>(path_.c_str()), 0);
    if (res >= 0) {
        uint32_t tbw = (texture_.Width + 63) / 64;
        texture_.TBW = tbw;
        uint32_t vramSize = tbw * 64 * texture_.Height * (getBpp(texture_.PSM) / 8);
        texture_.Vram = XYVramAllocator::alloc(gs, vramSize);
        if (texture_.Vram == 0) return false;

        if (texture_.Mem) {
            SyncDCache(texture_.Mem, (u8*)texture_.Mem + (texture_.Width * texture_.Height * (getBpp(texture_.PSM) / 8)));
        }

        gsKit_texture_upload(gs, &texture_);
        dmaKit_wait_fast();
        
        if (texture_.Mem) {
            ee_free(texture_.Mem);
            texture_.Mem = nullptr;
        }
        return true;
    }
    return false;
}

void XYImageJPG::unloadGS(GSGLOBAL* gs) {
    (void)gs;
    if (texture_.Vram != 0) {
        XYVramAllocator::free(texture_.Vram);
        texture_.Vram = 0;
    }
}

void XYImageJPG::freeEE() {
    if (texture_.Mem) {
        ee_free(texture_.Mem);
        texture_.Mem = nullptr;
    }
}

} // namespace xy
