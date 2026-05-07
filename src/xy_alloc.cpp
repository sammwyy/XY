// =============================================================================
// xy_alloc.cpp  —  XY PS2 Memory System Implementation
// =============================================================================

#include "xy_alloc.hpp"

#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <tamtypes.h>
#include <vector>
#include <kernel.h>  
#include <sifrpc.h>
#include <loadfile.h>
#include <iopheap.h>

namespace xy {

// =============================================================================
// Utilities
// =============================================================================

static void xy_assert_fail(const char* msg, const char* file, int line) {
    (void)msg; (void)file; (void)line;
#ifdef __mips__
    printf("XY ASSERT: %s  [%s:%d]\n", msg, file, line);
    // On PS2, halt rather than abort to keep the serial port alive.
    for (;;) {}
#else
    printf("XY ASSERT: %s  [%s:%d]\n", msg, file, line);
#endif
}

#define XY_ASSERT(cond, msg) \
    do { if (!(cond)) xy_assert_fail(msg, __FILE__, __LINE__); } while(0)

// =============================================================================
// MemTag names
// =============================================================================

const char* mem_tag_name(MemTag tag) {
    static const char* names[] = {
        "Untagged", "Graphics", "Audio",   "Physics",
        "AI",       "UI",       "Asset",   "Script",
        "Network",  "Temp",     "Debug",   "Game",
        "Vram",     "IOP",
    };
    size_t idx = static_cast<size_t>(tag);
    if (idx >= static_cast<size_t>(MemTag::_Count)) return "?";
    return names[idx];
}

// =============================================================================
// Leak Tracker (debug build only)
// =============================================================================

#ifdef XY_MEM_DEBUG

static LeakEntry s_leak_table[XY_LEAK_MAX_ENTRIES];
static int       s_leak_count = 0;

void mem_debug_record(void* ptr, size_t size, MemTag tag,
                      const char* file, int line) {
    if (s_leak_count >= XY_LEAK_MAX_ENTRIES) {
        printf("[MEM] leak table full, cannot record %p\n", ptr);
        return;
    }
    LeakEntry& e = s_leak_table[s_leak_count++];
    e.ptr  = ptr;
    e.size = size;
    e.tag  = tag;
    e.file = file;
    e.line = line;
    e.live = true;
}

void mem_debug_tag(void* ptr, MemTag tag, const char* file, int line) {
    for (int i = 0; i < s_leak_count; ++i) {
        if (s_leak_table[i].ptr == ptr && s_leak_table[i].live) {
            s_leak_table[i].tag  = tag;
            s_leak_table[i].file = file;
            s_leak_table[i].line = line;
            return;
        }
    }
}

void mem_debug_release(void* ptr) {
    for (int i = 0; i < s_leak_count; ++i) {
        if (s_leak_table[i].ptr == ptr && s_leak_table[i].live) {
            s_leak_table[i].live = false;
            return;
        }
    }
}

void mem_debug_dump_leaks() {
    int leaks = 0;
    printf("\n===== XY Memory Leak Report =====\n");
    for (int i = 0; i < s_leak_count; ++i) {
        if (s_leak_table[i].live) {
            printf("  LEAK  ptr=%p  size=%u  tag=%-10s  %s:%d\n",
                   s_leak_table[i].ptr,
                   (unsigned)s_leak_table[i].size,
                   mem_tag_name(s_leak_table[i].tag),
                   s_leak_table[i].file ? s_leak_table[i].file : "?",
                   s_leak_table[i].line);
            ++leaks;
        }
    }
    if (leaks == 0)
        printf("  No leaks detected.\n");
    printf("===================================\n\n");
}

#endif // XY_MEM_DEBUG

// =============================================================================
// XYArenaAllocator
// =============================================================================

XYArenaAllocator::XYArenaAllocator(void* buffer, size_t capacity)
    : base_(static_cast<uint8_t*>(buffer))
    , capacity_(capacity)
    , offset_(0)
    , owns_buffer_(false)
{
    XY_ASSERT(buffer != nullptr, "Arena: null buffer");
    XY_ASSERT(capacity > 0,     "Arena: zero capacity");
}

XYArenaAllocator::XYArenaAllocator(size_t capacity)
    : base_(nullptr)
    , capacity_(capacity)
    , offset_(0)
    , owns_buffer_(true)
{
    XY_ASSERT(capacity > 0, "Arena: zero capacity");
    // 64-byte align is fine for MIPS cache lines.
    base_ = static_cast<uint8_t*>(memalign(64, capacity));
    XY_ASSERT(base_ != nullptr, "Arena: allocation failed");
}

XYArenaAllocator::~XYArenaAllocator() {
    if (owns_buffer_ && base_) {
        ::free(base_);
        base_ = nullptr;
    }
}

void* XYArenaAllocator::alloc(size_t size, size_t alignment) {
    XY_ASSERT(is_power_of_two(alignment), "Arena: alignment must be power of two");
    size_t aligned_off = align_up(offset_, alignment);
    size_t new_off     = aligned_off + size;
    if (new_off > capacity_) {
        std::printf("[Arena] OOM: requested %lu, used %lu / %lu\n",
               (unsigned long)size, (unsigned long)offset_, (unsigned long)capacity_);
        return nullptr;
    }
    offset_ = new_off;
    return base_ + aligned_off;
}

void XYArenaAllocator::reset() {
    offset_ = 0;
}

void XYArenaAllocator::restore(size_t mark) {
    XY_ASSERT(mark <= offset_, "Arena: restore mark is ahead of current position");
    offset_ = mark;
}

// =============================================================================
// XYStackAllocator
// =============================================================================

XYStackAllocator::XYStackAllocator(size_t capacity)
    : base_(nullptr)
    , capacity_(capacity)
    , top_(0)
    , owns_buffer_(true)
{
    base_ = static_cast<uint8_t*>(memalign(64, capacity));
    XY_ASSERT(base_ != nullptr, "Stack: allocation failed");
}

XYStackAllocator::XYStackAllocator(void* buffer, size_t capacity)
    : base_(static_cast<uint8_t*>(buffer))
    , capacity_(capacity)
    , top_(0)
    , owns_buffer_(false)
{
    XY_ASSERT(buffer != nullptr, "Stack: null buffer");
}

XYStackAllocator::~XYStackAllocator() {
    if (owns_buffer_ && base_) {
        ::free(base_);
        base_ = nullptr;
    }
}

void* XYStackAllocator::alloc(size_t size, size_t alignment) {
    XY_ASSERT(is_power_of_two(alignment), "Stack: alignment must be power of two");

    // Reserve space for the BlockHeader before the user data.
    size_t header_size  = align_up(sizeof(BlockHeader), alignment);
    size_t aligned_top  = align_up(top_ + header_size, alignment);
    size_t new_top      = aligned_top + size;

    if (new_top > capacity_) {
        std::printf("[Stack] OOM: requested %lu, used %lu / %lu\n",
               (unsigned long)size, (unsigned long)top_, (unsigned long)capacity_);
        return nullptr;
    }

    // Write header just before the user pointer.
    BlockHeader* hdr = reinterpret_cast<BlockHeader*>(base_ + aligned_top - header_size);
    hdr->prev_top = top_;
    hdr->size     = size;
    hdr->guard    = STACK_GUARD;

    top_ = new_top;
    return base_ + aligned_top;
}

size_t XYStackAllocator::push() {
    return top_;
}

void XYStackAllocator::pop(size_t marker) {
    XY_ASSERT(marker <= top_, "Stack: pop marker is ahead of top");
    top_ = marker;
}

void XYStackAllocator::reset() {
    top_ = 0;
}

// =============================================================================
// XYScratchpadAllocator
// =============================================================================

XYScratchpadAllocator::XYScratchpadAllocator()
    : base_(reinterpret_cast<uint8_t*>(SPR_BASE))
    , offset_(0)
    , acquired_(false)
{}

XYScratchpadAllocator& XYScratchpadAllocator::instance() {
    static XYScratchpadAllocator inst;
    return inst;
}

bool XYScratchpadAllocator::acquire() {
    if (acquired_) {
        printf("[SPR] already acquired\n");
        return false;
    }
    acquired_ = true;
    offset_   = 0;
    return true;
}

void XYScratchpadAllocator::release() {
    acquired_ = false;
    offset_   = 0;
}

void* XYScratchpadAllocator::alloc(size_t size, size_t alignment) {
    XY_ASSERT(acquired_, "SPR: alloc without acquire()");
    XY_ASSERT(is_power_of_two(alignment), "SPR: alignment must be power of two");

    size_t aligned_off = align_up(offset_, alignment);
    size_t new_off     = aligned_off + size;

    if (new_off > XY_SCRATCH_SIZE) {
        std::printf("[SPR] OOM: requested %lu, used %lu / %lu\n",
               (unsigned long)size, (unsigned long)offset_, (unsigned long)XY_SCRATCH_SIZE);
        return nullptr;
    }

    offset_ = new_off;
    return base_ + aligned_off;
}

void XYScratchpadAllocator::reset() {
    XY_ASSERT(acquired_, "SPR: reset without acquire()");
    offset_ = 0;
}

// =============================================================================
// XYTransientAllocator
// =============================================================================

XYTransientAllocator::XYTransientAllocator(size_t slot_size, int buffer_count)
    : slot_size_(slot_size)
    , buffer_count_(buffer_count)
    , current_(0)
    , base_(nullptr)
    , offsets_(nullptr)
{
    XY_ASSERT(buffer_count >= 2, "Transient: must have at least 2 buffers");
    XY_ASSERT(slot_size    > 0,  "Transient: slot size must be > 0");

    size_t total = slot_size * static_cast<size_t>(buffer_count);
    base_    = static_cast<uint8_t*>(memalign(64, total));
    offsets_ = static_cast<size_t*>(memalign(16, sizeof(size_t) * static_cast<size_t>(buffer_count)));

    XY_ASSERT(base_    != nullptr, "Transient: base alloc failed");
    XY_ASSERT(offsets_ != nullptr, "Transient: offsets alloc failed");

    memset(offsets_, 0, sizeof(size_t) * static_cast<size_t>(buffer_count));
}

XYTransientAllocator::~XYTransientAllocator() {
    if (base_)    { ::free(base_);    base_    = nullptr; }
    if (offsets_) { ::free(offsets_); offsets_ = nullptr; }
}

void XYTransientAllocator::flip() {
    current_ = (current_ + 1) % buffer_count_;
    offsets_[current_] = 0;  // wipe the slot we just rotated into
}

void* XYTransientAllocator::alloc(size_t size, size_t alignment) {
    XY_ASSERT(is_power_of_two(alignment), "Transient: alignment must be power of two");

    size_t base_off    = static_cast<size_t>(current_) * slot_size_;
    size_t& slot_off   = offsets_[current_];
    size_t aligned_off = align_up(slot_off, alignment);
    size_t new_off     = aligned_off + size;

    if (new_off > slot_size_) {
        std::printf("[Transient] slot %d OOM: requested %lu, used %lu / %lu\n",
               current_, (unsigned long)size, (unsigned long)slot_off, (unsigned long)slot_size_);
        return nullptr;
    }

    slot_off = new_off;
    return base_ + base_off + aligned_off;
}

size_t XYTransientAllocator::used() const {
    return offsets_[current_];
}

// =============================================================================
// XYVramAllocator  (improved)
// =============================================================================

static const size_t VRAM_PAGE = 8192u;  // 8 KB GS page

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
    return instance().total_;
}

size_t XYVramAllocator::blockCount() {
    return instance().num_blocks_;
}

uint32_t XYVramAllocator::_alloc(GSGLOBAL* gs, uint32_t size) {
    uint32_t aligned_size = static_cast<uint32_t>(align_up(size, VRAM_PAGE));

    // Best-fit search among free blocks.
    int best = -1;
    for (size_t i = 0; i < num_blocks_; ++i) {
        if (blocks_[i].free && blocks_[i].size >= aligned_size) {
            if (best == -1 || blocks_[i].size < blocks_[best].size)
                best = static_cast<int>(i);
        }
    }

    if (best != -1) {
        // Split block if the remainder is large enough for at least one page.
        if (blocks_[best].size > aligned_size + VRAM_PAGE &&
            num_blocks_ < MAX_VRAM_BLOCKS)
        {
            Block tail;
            tail.address = blocks_[best].address + aligned_size;
            tail.size    = blocks_[best].size - aligned_size;
            tail.free    = true;
            blocks_[best].size = aligned_size;
            blocks_[num_blocks_++] = tail;
        }
        blocks_[best].free = false;
        used_bytes_ += blocks_[best].size;
        return blocks_[best].address;
    }

    // Bump allocation from the end of VRAM.
    if (next_ptr_ == 0) {
        next_ptr_ = static_cast<uint32_t>(align_up(gs->CurrentPointer, VRAM_PAGE));
    }

    uint32_t addr  = static_cast<uint32_t>(align_up(next_ptr_, VRAM_PAGE));
    uint32_t end   = addr + aligned_size;

    if (end > static_cast<uint32_t>(total_)) {
        std::printf("[VRAM] OOM: requested %lu, next=0x%08lx / 0x%08lx\n",
               (unsigned long)aligned_size, (unsigned long)next_ptr_, (unsigned long)total_);
        return 0;
    }

    if (num_blocks_ >= MAX_VRAM_BLOCKS) {
        printf("[VRAM] block table full\n");
        return 0;
    }

    blocks_[num_blocks_++] = {addr, aligned_size, false};
    used_bytes_ += aligned_size;
    next_ptr_    = end;
    gs->CurrentPointer = next_ptr_;

    return addr;
}

void XYVramAllocator::_free(uint32_t address) {
    for (size_t i = 0; i < num_blocks_; ++i) {
        if (blocks_[i].address == address && !blocks_[i].free) {
            blocks_[i].free = true;
            used_bytes_ -= blocks_[i].size;
            _coalesce();
            return;
        }
    }
    std::printf("[VRAM] free: unknown address 0x%08lx\n", (unsigned long)address);
}

void XYVramAllocator::_clear(GSGLOBAL* gs) {
    (void)gs;
    num_blocks_ = 0;
    next_ptr_   = 0;
    used_bytes_ = 0;
}

// Merge adjacent free blocks to reduce fragmentation.
void XYVramAllocator::_coalesce() {
    // Simple O(n²) coalesce — block count is small (≤128), fine for PS2.
    bool merged = true;
    while (merged) {
        merged = false;
        for (size_t i = 0; i < num_blocks_; ++i) {
            if (!blocks_[i].free) continue;
            for (size_t j = 0; j < num_blocks_; ++j) {
                if (i == j || !blocks_[j].free) continue;
                if (blocks_[i].address + blocks_[i].size == blocks_[j].address) {
                    // Merge j into i.
                    blocks_[i].size += blocks_[j].size;
                    // Remove j by swapping with the last.
                    blocks_[j] = blocks_[--num_blocks_];
                    merged = true;
                    break;
                }
            }
            if (merged) break;
        }
    }
}

// =============================================================================
// XYHeapAllocator
// =============================================================================

XYHeapAllocator& XYHeapAllocator::instance() {
    static XYHeapAllocator inst;
    return inst;
}

void* XYHeapAllocator::alloc(size_t size, size_t alignment) {
    return instance()._alloc(size, MemTag::Untagged, nullptr, 0, alignment);
}

void* XYHeapAllocator::alloc_tagged(size_t size, MemTag tag,
                                    const char* file, int line,
                                    size_t alignment) {
    return instance()._alloc(size, tag, file, line, alignment);
}

void XYHeapAllocator::free(void* ptr) {
    instance()._free(ptr);
}

size_t XYHeapAllocator::usedBytes() {
    return instance().used_bytes_;
}

size_t XYHeapAllocator::usedByTag(MemTag tag) {
    size_t idx = static_cast<size_t>(tag);
    if (idx >= static_cast<size_t>(MemTag::_Count)) return 0;
    return instance().used_by_tag_[idx];
}

void XYHeapAllocator::dumpStats() {
    printf("\n===== XY Heap Stats =====\n");
    printf("  Total used: %u bytes\n", (unsigned)instance().used_bytes_);
    for (size_t i = 0; i < static_cast<size_t>(MemTag::_Count); ++i) {
        size_t u = instance().used_by_tag_[i];
        if (u > 0)
            printf("  %-10s : %u bytes\n", mem_tag_name(static_cast<MemTag>(i)), (unsigned)u);
    }
    printf("===========================\n\n");
}

void* XYHeapAllocator::_alloc(size_t size, MemTag tag,
                               const char* file, int line,
                               size_t alignment) {
    // Ensure header doesn't break the user's alignment.
    size_t header_sz    = align_up(sizeof(MemHeader), alignment);
    size_t total_sz     = header_sz + size;

    void* raw = memalign(alignment, total_sz);
    if (!raw) {
        std::printf("[Heap] OOM: requested %lu bytes (tag=%s)\n",
               (unsigned long)size, mem_tag_name(tag));
        return nullptr;
    }

    MemHeader* hdr = static_cast<MemHeader*>(raw);
    hdr->magic  = MEM_MAGIC;
    hdr->size   = size;
    hdr->tag    = tag;
    memset(hdr->_pad, 0, sizeof(hdr->_pad));
#ifdef XY_MEM_DEBUG
    hdr->file   = file;
    hdr->line   = line;
#else
    (void)file; (void)line;
#endif

    used_bytes_            += size;
    used_by_tag_[static_cast<size_t>(tag)] += size;

    void* user_ptr = reinterpret_cast<uint8_t*>(raw) + header_sz;

#ifdef XY_MEM_DEBUG
    mem_debug_record(user_ptr, size, tag, file, line);
#endif

    return user_ptr;
}

void XYHeapAllocator::_free(void* ptr) {
    if (!ptr) return;

    // Locate the header: we stored it at an alignment-padded distance.
    // Try standard sizeof(MemHeader) offset first, then fall back to scan.
    // In practice alignment is always the same, so direct offset is reliable.
    // We use alignment=128 by default, so header_sz = 128.
    static const size_t DEFAULT_HEADER_SZ = align_up(sizeof(MemHeader), 128u);

    MemHeader* hdr = reinterpret_cast<MemHeader*>(
        reinterpret_cast<uint8_t*>(ptr) - DEFAULT_HEADER_SZ);

    if (hdr->magic != MEM_MAGIC) {
        // Attempt fallback: raw memalign pointer passed directly.
        std::printf("[Heap] free: bad magic for ptr=%p, trying raw free\n", ptr);
        hdr->magic = MEM_DEAD;
        ::free(ptr);
        return;
    }

    used_bytes_ -= hdr->size;
    size_t tag_idx = static_cast<size_t>(hdr->tag);
    if (tag_idx < static_cast<size_t>(MemTag::_Count))
        used_by_tag_[tag_idx] -= hdr->size;

#ifdef XY_MEM_DEBUG
    mem_debug_release(ptr);
#endif

    hdr->magic = MEM_DEAD;
    ::free(hdr);  // free from the raw memalign'd base
}

// =============================================================================
// C-style wrappers
// =============================================================================

void* ee_alloc(size_t size) {
    return XYHeapAllocator::alloc(size);
}

void ee_free(void* ptr) {
    XYHeapAllocator::free(ptr);
}

// =============================================================================
// Global frame allocator
// =============================================================================

XYStackAllocator& get_frame_allocator() {
    static XYStackAllocator frame_alloc(XY_FRAME_STACK_SIZE);
    return frame_alloc;
}

void frame_alloc_reset() {
    get_frame_allocator().reset();
}

void* frame_alloc(size_t size, size_t alignment) {
    return get_frame_allocator().alloc(size, alignment);
}

// =============================================================================
// XYIopAllocator Implementation
// =============================================================================

XYIopAllocator& XYIopAllocator::instance() {
    static XYIopAllocator inst;
    return inst;
}

uint32_t XYIopAllocator::alloc(size_t size) {
    return instance()._alloc(size);
}

void XYIopAllocator::free(uint32_t address) {
    instance()._free(address);
}

size_t XYIopAllocator::usedBytes() {
    return instance().used_bytes_;
}

uint32_t XYIopAllocator::_alloc(size_t size) {
    // SifAllocIopHeap handles the actual IOP-side allocation.
    void* ptr = SifAllocIopHeap(static_cast<int>(size));
    if (!ptr) {
        std::printf("[IOP] OOM: requested %lu bytes\n", (unsigned long)size);
        return 0;
    }

    uint32_t addr = reinterpret_cast<uintptr_t>(ptr);
    blocks_.push_back({addr, size});
    used_bytes_ += size;
    return addr;
}

void XYIopAllocator::_free(uint32_t address) {
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].address == address) {
            SifFreeIopHeap(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
            used_bytes_ -= blocks_[i].size;
            blocks_.erase(blocks_.begin() + i);
            return;
        }
    }
}

} // namespace xy

// =============================================================================
// Global Overrides
// =============================================================================

#if defined(XY_OVERRIDE_NEW)

void* operator new(size_t size) {
    return xy::XYHeapAllocator::alloc(size);
}

void* operator new[](size_t size) {
    return xy::XYHeapAllocator::alloc(size);
}

void operator delete(void* ptr) noexcept {
    xy::XYHeapAllocator::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    xy::XYHeapAllocator::free(ptr);
}

void* operator new(size_t size, xy::MemTag tag, const char* file, int line) {
    return xy::XYHeapAllocator::alloc_tagged(size, tag, file, line);
}

void* operator new[](size_t size, xy::MemTag tag, const char* file, int line) {
    return xy::XYHeapAllocator::alloc_tagged(size, tag, file, line);
}

#endif
