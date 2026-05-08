#include "xy_image.hpp"
#include "../xy_alloc.hpp"
#include <gsToolkit.h>
#include <malloc.h>
#include <cstring>
#include <dmaKit.h>
#include <kernel.h>

namespace xy {

XYImagePNG::XYImagePNG() {
    std::memset(&texture_, 0, sizeof(texture_));
    texture_.Delayed = 1;
}

XYImagePNG::~XYImagePNG() {
}

bool XYImagePNG::loadEE(const std::string& path) {
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

bool XYImagePNG::loadGS(GSGLOBAL* gs) {
    if (texture_.Vram != 0) return true;

    int res = gsKit_texture_png(gs, &texture_, const_cast<char*>(path_.c_str()));
    if (res >= 0) {
        std::printf("[XYImagePNG] gsKit_texture_png success: %dx%d, PSM: %d\n", 
                   texture_.Width, texture_.Height, texture_.PSM);
        if (texture_.Vram == 0) {
            uint32_t tbw = (texture_.Width + 63) / 64;
            texture_.TBW = tbw;
            uint32_t bpp = getBpp(texture_.PSM);
            uint32_t vramSize = (tbw * 64 * texture_.Height * bpp + 7) / 8;
            
            std::printf("[XYImagePNG] Loading %s (%dx%d, PSM: %d, Size: %lu)\n", 
                       path_.c_str(), texture_.Width, texture_.Height, texture_.PSM, vramSize);

            texture_.Vram = XYVramAllocator::alloc(gs, vramSize);
            if (texture_.Vram == 0) {
                std::printf("[XYImagePNG] VRAM allocation failed for %s\n", path_.c_str());
                return false;
            }
        }

        if (texture_.PSM == GS_PSM_T8 || texture_.PSM == GS_PSM_T4) {
            int clutWidth = (texture_.PSM == GS_PSM_T8) ? 16 : 8;
            int clutHeight = (texture_.PSM == GS_PSM_T8) ? 16 : 2;
            uint32_t clutSize = gsKit_texture_size(clutWidth, clutHeight, GS_PSM_CT32);
            texture_.VramClut = XYVramAllocator::alloc(gs, clutSize);
            if (texture_.VramClut == 0) {
                std::printf("[XYImagePNG] VRAM CLUT allocation failed for %s\n", path_.c_str());
                return false;
            }
        }

        if (texture_.Mem) {
            SyncDCache(texture_.Mem, (u8*)texture_.Mem + (texture_.Width * texture_.Height * (getBpp(texture_.PSM) / 8)));
        }

        if (texture_.Clut) {
            int clutWidth = (texture_.PSM == GS_PSM_T8) ? 16 : 8;
            int clutHeight = (texture_.PSM == GS_PSM_T8) ? 16 : 2;
            SyncDCache(texture_.Clut, (u8*)texture_.Clut + (clutWidth * clutHeight * 4));
        }

        gsKit_texture_upload(gs, &texture_);
        dmaKit_wait_fast();
        
        std::printf("[XYImagePNG] Uploaded %s to VRAM 0x%08X\n", path_.c_str(), texture_.Vram);

        if (texture_.Mem) {
            std::free(texture_.Mem);
            texture_.Mem = nullptr;
        }
        
        if (texture_.Clut) {
            std::free(texture_.Clut);
            texture_.Clut = nullptr;
        }
        
        return true;
    }

    std::printf("[XYImagePNG] gsKit_texture_png failed for %s (res: %d)\n", path_.c_str(), res);
    return false;
}

void XYImagePNG::unloadGS(GSGLOBAL* gs) {
    (void)gs;
    if (texture_.Vram != 0) {
        XYVramAllocator::free(texture_.Vram);
        texture_.Vram = 0; 
    }
    if (texture_.VramClut != 0) {
        XYVramAllocator::free(texture_.VramClut);
        texture_.VramClut = 0;
    }
}

void XYImagePNG::freeEE() {
    if (texture_.Mem) {
        ee_free(texture_.Mem);
        texture_.Mem = nullptr;
    }
}

} // namespace xy
