#include "xy_alloc.hpp"
#include <malloc.h>
#include <cstring>

namespace xy {

static const uint32_t MEM_MAGIC = 0x58594F4E;

// --- VRAM Allocator ---

XYVramAllocator& XYVramAllocator::instance() {
    static XYVramAllocator inst;
    return inst;
}

uint32_t XYVramAllocator::alloc(GSGLOBAL* gs, uint32_t size) {
    return instance()._alloc(gs, size);
}

void XYVramAllocator::free(uint32_t address) {
    instance()._free(address);
}

void XYVramAllocator::clear(GSGLOBAL* gs) {
    instance()._clear(gs);
}

size_t XYVramAllocator::usedBytes() {
    return instance().used_bytes_;
}

size_t XYVramAllocator::totalBytes() {
    return 0x400000;
}

uint32_t XYVramAllocator::_alloc(GSGLOBAL* gs, uint32_t size) {
    uint32_t aligned_size = (size + 8191) & ~8191;
    
    int best_idx = -1;
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].free && blocks_[i].size >= aligned_size) {
            if (best_idx == -1 || blocks_[i].size < blocks_[best_idx].size) {
                best_idx = i;
            }
        }
    }

    if (best_idx != -1) {
        blocks_[best_idx].free = false;
        used_bytes_ += blocks_[best_idx].size;
        return blocks_[best_idx].address;
    }

    if (next_ptr_ == 0) {
        next_ptr_ = (gs->CurrentPointer + 8191) & ~8191;
    }

    uint32_t addr = (next_ptr_ + 8191) & ~8191;
    next_ptr_ = addr + aligned_size;
    
    if (next_ptr_ > 0x400000) {
        // VRAM Overflow!
        return 0;
    }

    blocks_.push_back({addr, aligned_size, false});
    used_bytes_ += aligned_size;
    gs->CurrentPointer = next_ptr_;
    
    return addr;
}

void XYVramAllocator::_free(uint32_t address) {
    for (auto& block : blocks_) {
        if (block.address == address && !block.free) {
            block.free = true;
            used_bytes_ -= block.size;
            return;
        }
    }
}

void XYVramAllocator::_clear(GSGLOBAL* gs) {
    (void)gs;
    blocks_.clear();
    next_ptr_ = 0;
    used_bytes_ = 0;
}

// --- EE Allocator ---

XYEeAllocator& XYEeAllocator::instance() {
    static XYEeAllocator inst;
    return inst;
}

void* XYEeAllocator::alloc(size_t size) {
    return instance()._alloc(size);
}

void XYEeAllocator::free(void* ptr) {
    instance()._free(ptr);
}

size_t XYEeAllocator::usedBytes() {
    return instance().used_bytes_;
}

void* XYEeAllocator::_alloc(size_t size) {
    size_t total_size = size + sizeof(MemHeader);
    void* ptr = memalign(128, total_size);
    if (!ptr) return nullptr;

    MemHeader* header = static_cast<MemHeader*>(ptr);
    header->size = size;
    header->magic = MEM_MAGIC;

    used_bytes_ += size;
    return static_cast<void*>(header + 1);
}

void XYEeAllocator::_free(void* ptr) {
    if (!ptr) return;

    MemHeader* header = static_cast<MemHeader*>(ptr) - 1;
    if (header->magic != MEM_MAGIC) {
        ::free(ptr);
        return;
    }

    used_bytes_ -= header->size;
    header->magic = 0;
    ::free(header);
}

void* ee_alloc(size_t size) {
    return XYEeAllocator::alloc(size);
}

void ee_free(void* ptr) {
    XYEeAllocator::free(ptr);
}

} // namespace xy
