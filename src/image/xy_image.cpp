#include "xy_image.hpp"
#include <algorithm>

namespace xy {

// --- XYTexture ---

XYTexture::XYTexture() : image_(nullptr) {}
XYTexture::~XYTexture() {
    free();
}

bool XYTexture::load(GSGLOBAL* gs, const std::string& path) {
    path_ = path;
    image_ = XYImageManager::instance().load(gs, path);
    return valid();
}

void XYTexture::unload(GSGLOBAL* gs) {
    if (image_) {
        image_->unloadGS(gs);
    }
}

void XYTexture::free() {
    if (!path_.empty()) {
        XYImageManager::instance().free(path_, nullptr);
        image_ = nullptr;
        path_ = "";
    }
}

bool XYTexture::swapIn(GSGLOBAL* gs) {
    if (!image_) return false;
    return image_->loadGS(gs);
}

void XYTexture::swapOut(GSGLOBAL* gs) {
    if (image_) {
        image_->unloadGS(gs);
    }
}

bool XYTexture::isResidentGS() const {
    return image_ && image_->isResidentGS();
}

bool XYTexture::isResidentEE() const {
    return image_ && image_->isResidentEE();
}

int XYTexture::width() const {
    return image_ ? image_->width() : 0;
}

int XYTexture::height() const {
    return image_ ? image_->height() : 0;
}

bool XYTexture::valid() const {
    return image_ && image_->valid();
}

GSTEXTURE* XYTexture::raw() {
    return image_ ? image_->raw() : nullptr;
}

const std::string& XYTexture::path() const {
    return path_;
}

// --- XYImageManager ---

XYImageManager& XYImageManager::instance() {
    static XYImageManager inst;
    return inst;
}

std::shared_ptr<XYImage> XYImageManager::load(GSGLOBAL* gs, const std::string& path, Memory target) {
    auto it = registry_.find(path);
    if (it != registry_.end()) {
        if (target == Memory::GS && gs) {
            it->second->loadGS(gs);
        }
        return it->second;
    }

    std::shared_ptr<XYImage> img = nullptr;
    std::string ext = path.substr(path.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "png") {
        img = std::make_shared<XYImagePNG>();
    } else if (ext == "jpg" || ext == "jpeg") {
        img = std::make_shared<XYImageJPG>();
    } else if (ext == "p2t" || ext == "ps2tex" || ext == "tex") {
        img = std::make_shared<XYImageP2T>();
    }

    if (img) {
        img->loadEE(path);
        if (target == Memory::GS && gs) {
            img->loadGS(gs);
        }
        registry_[path] = img;
    }

    return img;
}

void XYImageManager::unload(const std::string& path, GSGLOBAL* gs) {
    auto it = registry_.find(path);
    if (it != registry_.end()) {
        it->second->unloadGS(gs);
    }
}

void XYImageManager::free(const std::string& path, GSGLOBAL* gs) {
    auto it = registry_.find(path);
    if (it != registry_.end()) {
        it->second->unloadGS(gs);
        it->second->freeEE();
        registry_.erase(it);
    }
}

} // namespace xy
