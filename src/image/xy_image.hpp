#pragma once

#include <gsKit.h>
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace xy {

enum class Memory {
    EE,
    GS
};

class XYImage {
public:
    virtual ~XYImage() {}

    virtual bool loadEE(const std::string& path) = 0;
    virtual bool loadGS(GSGLOBAL* gs) = 0;
    virtual void unloadGS(GSGLOBAL* gs) = 0;
    virtual void freeEE() = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual GSTEXTURE* raw() = 0;
    
    virtual bool valid() const = 0;
    virtual bool isResidentEE() const = 0;
    virtual bool isResidentGS() const = 0;
};

class XYImagePNG : public XYImage {
public:
    XYImagePNG();
    ~XYImagePNG() override;

    bool loadEE(const std::string& path) override;
    bool loadGS(GSGLOBAL* gs) override;
    void unloadGS(GSGLOBAL* gs) override;
    void freeEE() override;

    int width() const override { return texture_.Width; }
    int height() const override { return texture_.Height; }
    GSTEXTURE* raw() override { return &texture_; }
    bool valid() const override { return texture_.Vram != 0; }
    bool isResidentEE() const override { return pixels_ != nullptr; }
    bool isResidentGS() const override { return texture_.Vram != 0; }

private:
    GSTEXTURE texture_;
    std::string path_;
    void* pixels_;
};

enum Ps2TexFlags {
    P2TX_SWIZZLED      = 1 << 0,
    P2TX_CLUT_ROTATED  = 1 << 1,
    P2TX_HAS_ALPHA     = 1 << 2,
};

struct Ps2TexHeader {
    char magic[4];        // "P2TX"
    uint16_t version;     // 1
    uint16_t width;
    uint16_t height;
    uint16_t orig_width;
    uint16_t orig_height;

    uint8_t psm;          // GS pixel storage mode
    uint8_t has_clut;
    uint8_t clut_psm;
    uint8_t mip_count;

    uint32_t data_size;
    uint32_t clut_size;

    uint32_t data_offset;
    uint32_t clut_offset;

    uint32_t flags;       // swizzled, atlas, alpha, etc.
    uint8_t reserved[6];
} __attribute__((packed));

static_assert(sizeof(Ps2TexHeader) == 44);

class XYImageP2T : public XYImage {
public:
    XYImageP2T();
    ~XYImageP2T() override;

    bool loadEE(const std::string& path) override;
    bool loadGS(GSGLOBAL* gs) override;
    void unloadGS(GSGLOBAL* gs) override;
    void freeEE() override;

    int width() const override { return texture_.Width; }
    int height() const override { return texture_.Height; }
    GSTEXTURE* raw() override { return &texture_; }
    bool valid() const override { return texture_.Vram != 0; }
    bool isResidentEE() const override { return texture_.Mem != nullptr; }
    bool isResidentGS() const override { return texture_.Vram != 0; }

private:
    GSTEXTURE texture_;
    std::string path_;
    Ps2TexHeader header_;
};

class XYImageJPG : public XYImage {
public:
    XYImageJPG();
    ~XYImageJPG() override;

    bool loadEE(const std::string& path) override;
    bool loadGS(GSGLOBAL* gs) override;
    void unloadGS(GSGLOBAL* gs) override;
    void freeEE() override;

    int width() const override { return texture_.Width; }
    int height() const override { return texture_.Height; }
    GSTEXTURE* raw() override { return &texture_; }
    bool valid() const override { return texture_.Vram != 0; }
    bool isResidentEE() const override { return texture_.Mem != nullptr; }
    bool isResidentGS() const override { return texture_.Vram != 0; }

private:
    GSTEXTURE texture_;
    std::string path_;
};

class XYImageManager {
public:
    static XYImageManager& instance();

    std::shared_ptr<XYImage> load(GSGLOBAL* gs, const std::string& path, Memory target = Memory::GS);
    void unload(const std::string& path, GSGLOBAL* gs);
    void free(const std::string& path, GSGLOBAL* gs);

private:
    XYImageManager() {}
    std::map<std::string, std::shared_ptr<XYImage>> registry_;
};

class XYTexture {
public:
    XYTexture();
    ~XYTexture();

    bool load(GSGLOBAL* gs, const std::string& path);
    void unload(GSGLOBAL* gs);
    void free();

    // Swapping support
    bool swapIn(GSGLOBAL* gs);
    void swapOut(GSGLOBAL* gs);
    bool isResidentGS() const;
    bool isResidentEE() const;

    int width() const;
    int height() const;
    bool valid() const;
    GSTEXTURE* raw();
    const std::string& path() const;

private:
    std::shared_ptr<XYImage> image_;
    std::string path_;
};

} // namespace xy
