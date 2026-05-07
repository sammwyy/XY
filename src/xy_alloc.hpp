#pragma once

#include <gsKit.h>
#include <cstdint>
#include <vector>
#include <cstddef>

namespace xy {

class XYVramAllocator {
public:
    static uint32_t alloc(GSGLOBAL* gs, uint32_t size);
    static void free(uint32_t address);
    static void clear(GSGLOBAL* gs);
    static size_t usedBytes();
    static size_t totalBytes();

private:
    XYVramAllocator() : next_ptr_(0), used_bytes_(0) {}
    static XYVramAllocator& instance();

    uint32_t _alloc(GSGLOBAL* gs, uint32_t size);
    void _free(uint32_t address);
    void _clear(GSGLOBAL* gs);

    struct Block {
        uint32_t address;
        uint32_t size;
        bool free;
    };

    uint32_t next_ptr_;
    size_t used_bytes_;
    std::vector<Block> blocks_;
};

class XYEeAllocator {
public:
    static void* alloc(size_t size);
    static void free(void* ptr);
    static size_t usedBytes();

private:
    XYEeAllocator() : used_bytes_(0) {}
    static XYEeAllocator& instance();

    void* _alloc(size_t size);
    void _free(void* ptr);
    
    struct MemHeader {
        size_t size;
        uint32_t magic;
    };

    size_t used_bytes_;
};

void* ee_alloc(size_t size);
void ee_free(void* ptr);

} // namespace xy
