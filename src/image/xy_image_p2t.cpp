#include "xy_image.hpp"
#include "../xy_alloc.hpp"
#include <gsToolkit.h>
#include <malloc.h>
#include <cstring>
#include <dmaKit.h>
#include <kernel.h>
#include <cstdio>

namespace xy {

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

XYImageP2T::XYImageP2T() {
    std::memset(&texture_, 0, sizeof(texture_));
    std::memset(&header_, 0, sizeof(header_));
}

XYImageP2T::~XYImageP2T() {
    freeEE();
}

bool XYImageP2T::loadEE(const std::string& path) {
    path_ = path;
    
    FILE* fp = std::fopen(path_.c_str(), "rb");
    if (!fp) return false;

    if (std::fread(&header_, 1, sizeof(header_), fp) != sizeof(header_)) {
        std::fclose(fp);
        return false;
    }

    if (std::strncmp(header_.magic, "P2TX", 4) != 0) {
        std::fclose(fp);
        return false;
    }

    // Allocate and read pixel data
    texture_.Mem = (u32*)ee_alloc(header_.data_size);
    std::fseek(fp, header_.data_offset, SEEK_SET);
    std::fread(texture_.Mem, 1, header_.data_size, fp);

    // Allocate and read CLUT if present
    if (header_.has_clut) {
        texture_.Clut = (u32*)ee_alloc(header_.clut_size);
        std::fseek(fp, header_.clut_offset, SEEK_SET);
        std::fread(texture_.Clut, 1, header_.clut_size, fp);
    }

    std::fclose(fp);
    
    // We don't initialize the full GSTEXTURE yet, loadGS will do that.
    return true;
}

bool XYImageP2T::loadGS(GSGLOBAL* gs) {
    if (texture_.Vram != 0) return true;

    // If data is not in RAM, load it from file now (lazy load).
    if (!texture_.Mem) {
        if (!loadEE(path_)) return false;
    }

    // Initialize GSTEXTURE structure from header
    texture_.Width = header_.width;
    texture_.Height = header_.height;
    texture_.PSM = header_.psm;
    texture_.ClutPSM = header_.clut_psm;
    texture_.Delayed = 1;
    
    uint32_t tbw = (texture_.Width + 63) / 64;
    texture_.TBW = tbw;
    
    uint32_t bpp = getBpp(texture_.PSM);
    uint32_t vramSize = (tbw * 64 * texture_.Height * bpp + 7) / 8;

    std::printf("[P2TX] Loading GS %s (%dx%d, PSM: %d)\n", 
               path_.c_str(), texture_.Width, texture_.Height, texture_.PSM);

    texture_.Vram = XYVramAllocator::alloc(gs, vramSize);
    if (texture_.Vram == 0) {
        return false;
    }

    if (header_.has_clut) {
        uint32_t clutEntries = (texture_.PSM == GS_PSM_T8) ? 256 : 16;
        uint32_t clutVramSize = (clutEntries * 4 + 63) & ~63;
        texture_.VramClut = XYVramAllocator::alloc(gs, clutVramSize);
    }

    // Sync DCache before DMA upload
    if (texture_.Mem) {
        SyncDCache(texture_.Mem, (u8*)texture_.Mem + header_.data_size);
    }

    if (texture_.Clut) {
        SyncDCache(texture_.Clut, (u8*)texture_.Clut + header_.clut_size);
    }

    // Upload to GS
    gsKit_texture_upload(gs, &texture_);
    dmaKit_wait_fast();

    return true;
}

void XYImageP2T::unloadGS(GSGLOBAL* gs) {
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

void XYImageP2T::freeEE() {
    if (texture_.Mem) {
        ee_free(texture_.Mem);
        texture_.Mem = nullptr;
    }
    if (texture_.Clut) {
        ee_free(texture_.Clut);
        texture_.Clut = nullptr;
    }
}

} // namespace xy
