#include "xy_sound.hpp"
#include <algorithm>
#include <cstdio>

namespace xy {

// ---------------------------------------------------------------------------
// XYSoundClip
// ---------------------------------------------------------------------------

XYSoundClip::XYSoundClip() : sound_(nullptr) {}
XYSoundClip::~XYSoundClip() { free(); }

bool XYSoundClip::load(const std::string& path) {
    path_ = path;
    sound_ = XYSoundManager::instance().load(path);
    return valid();
}

void XYSoundClip::unload() {
    if (sound_) {
        sound_->free();
    }
}

void XYSoundClip::free() {
    if (!path_.empty()) {
        XYSoundManager::instance().unload(path_);
        sound_ = nullptr;
        path_ = "";
    }
}

int  XYSoundClip::sampleRate()  const { return sound_ ? sound_->sampleRate()  : 0; }
int  XYSoundClip::channels()    const { return sound_ ? sound_->channels()    : 0; }
int  XYSoundClip::sampleCount() const { return sound_ ? sound_->sampleCount() : 0; }
bool XYSoundClip::looping()     const { return sound_ ? sound_->looping()     : false; }
int  XYSoundClip::loopStart()   const { return sound_ ? sound_->loopStart()   : 0; }
int  XYSoundClip::loopEnd()     const { return sound_ ? sound_->loopEnd()     : 0; }
bool XYSoundClip::valid()       const { return sound_ && sound_->valid(); }

const s16* XYSoundClip::samples()     const { return sound_ ? sound_->samples()     : nullptr; }
int        XYSoundClip::samplesSize() const { return sound_ ? sound_->samplesSize() : 0; }

// ---------------------------------------------------------------------------
// XYSoundManager
// ---------------------------------------------------------------------------

XYSoundManager& XYSoundManager::instance() {
    static XYSoundManager inst;
    return inst;
}

std::shared_ptr<XYSoundData> XYSoundManager::load(const std::string& path) {
    auto it = registry_.find(path);
    if (it != registry_.end()) {
        return it->second;
    }

    // Determine format from extension
    std::string ext = path.substr(path.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::shared_ptr<XYSoundData> snd = nullptr;

    if (ext == "wav") {
        snd = std::make_shared<XYSoundWAV>();
    } else if (ext == "snd" || ext == "ps2snd" || ext == "p2s") {
        snd = std::make_shared<XYSoundSND>();
    }

    if (snd) {
        if (snd->load(path)) {
            registry_[path] = snd;
        } else {
            std::printf("[XYSound] Failed to load: %s\n", path.c_str());
            snd = nullptr;
        }
    } else {
        std::printf("[XYSound] Unknown sound format: %s\n", ext.c_str());
    }

    return snd;
}

void XYSoundManager::unload(const std::string& path) {
    auto it = registry_.find(path);
    if (it != registry_.end()) {
        it->second->free();
        registry_.erase(it);
    }
}

void XYSoundManager::clear() {
    for (auto& kv : registry_) {
        kv.second->free();
    }
    registry_.clear();
}

} // namespace xy
