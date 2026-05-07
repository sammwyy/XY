// =============================================================================
// xy_alloc.hpp  —  XY PS2 Memory System
// =============================================================================
//
//  Allocators available:
//
//    XYArenaAllocator         — linear bump-pointer, resets as a whole
//    XYStackAllocator         — push/pop with save-points (temp per-frame)
//    XYPoolAllocator<T,N>     — fixed-size object pool, O(1) alloc/free
//    XYScratchpadAllocator    — EE scratch-pad RAM (SPR, 16 KB)
//    XYVramAllocator          — GS VRAM, best-fit with block coalescing
//    XYTransientAllocator     — double/triple buffered frame memory
//    XYHeapAllocator          — general-purpose (wraps memalign), with tags
//
//  Leak tracker (XY_MEM_DEBUG):
//    All tagged allocs register source file + line. Call
//    XY_MEM_DUMP_LEAKS() at shutdown.
//
// =============================================================================

#pragma once

#include <gsKit.h>
#include <cstdint>
#include <vector>
#include <cstddef>
#include <cstring>
#include <new>

// ─── compile-time configuration ─────────────────────────────────────────────

#ifndef XY_ARENA_DEFAULT_SIZE
#  define XY_ARENA_DEFAULT_SIZE   (2u * 1024u * 1024u)   // 2 MB
#endif

#ifndef XY_FRAME_STACK_SIZE
#  define XY_FRAME_STACK_SIZE     (512u * 1024u)          // 512 KB per frame slot
#endif

#ifndef XY_SCRATCH_SIZE
#  define XY_SCRATCH_SIZE         (16u * 1024u)           // SPR is exactly 16 KB
#endif

#ifndef XY_TRANSIENT_BUFFERS
#  define XY_TRANSIENT_BUFFERS    2                       // double-buffer by default
#endif

#ifndef XY_POOL_MAX_CLASSES
#  define XY_POOL_MAX_CLASSES     16
#endif

#ifndef XY_LEAK_MAX_ENTRIES
#  define XY_LEAK_MAX_ENTRIES     512
#endif

// ─── debug helpers ──────────────────────────────────────────────────────────

#ifdef XY_MEM_DEBUG
#  define XY_MEM_TAG(ptr, tag)   ::xy::mem_debug_tag(ptr, tag, __FILE__, __LINE__)
#  define XY_MEM_DUMP_LEAKS()    ::xy::mem_debug_dump_leaks()
#else
#  define XY_MEM_TAG(ptr, tag)   ((void)0)
#  define XY_MEM_DUMP_LEAKS()    ((void)0)
#endif

// ─── convenience macros ─────────────────────────────────────────────────────

#define XY_ALLOC(size)          ::xy::XYHeapAllocator::alloc(size)
#define XY_FREE(ptr)            ::xy::XYHeapAllocator::free(ptr)
#define XY_ALLOC_TAG(sz, tag)   ::xy::XYHeapAllocator::alloc_tagged(sz, tag)

#define XY_ARENA_ALLOC(a, sz)   (a).alloc(sz)
#define XY_POOL_ALLOC(p)        (p).alloc()
#define XY_POOL_FREE(p, ptr)    (p).free(ptr)

// ─── alignment helpers ──────────────────────────────────────────────────────

namespace xy {

static inline constexpr size_t align_up(size_t value, size_t align) {
    return (value + align - 1u) & ~(align - 1u);
}

static inline constexpr bool is_power_of_two(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

// ─── Memory Tags ────────────────────────────────────────────────────────────
//  Coarse tags identifying which engine system owns an allocation.

enum class MemTag : uint8_t {
    Untagged   = 0,
    Graphics   = 1,
    Audio      = 2,
    Physics    = 3,
    AI         = 4,
    UI         = 5,
    Asset      = 6,
    Script     = 7,
    Network    = 8,
    Temp       = 9,
    Debug      = 10,
    Game       = 11,
    Vram       = 12,
    IOP        = 13,
    _Count     = 14,
};

const char* mem_tag_name(MemTag tag);

// ─── Leak Tracker (debug only) ──────────────────────────────────────────────

#ifdef XY_MEM_DEBUG
struct LeakEntry {
    void*       ptr;
    size_t      size;
    MemTag      tag;
    const char* file;
    int         line;
    bool        live;
};

void mem_debug_tag(void* ptr, MemTag tag, const char* file, int line);
void mem_debug_record(void* ptr, size_t size, MemTag tag,
                      const char* file, int line);
void mem_debug_release(void* ptr);
void mem_debug_dump_leaks();
#endif // XY_MEM_DEBUG

// =============================================================================
// XYArenaAllocator
// =============================================================================
//  Linear bump-pointer allocator. Allocs are O(1), no per-alloc overhead.
//  The entire arena is released at once with reset().
//
//  Typical use: level/scene lifetime allocations.
// =============================================================================

class XYArenaAllocator {
public:
    // Construct owning an external buffer (no heap).
    XYArenaAllocator(void* buffer, size_t capacity);

    // Construct and allocate backing store from the heap.
    explicit XYArenaAllocator(size_t capacity = XY_ARENA_DEFAULT_SIZE);

    ~XYArenaAllocator();

    XYArenaAllocator(const XYArenaAllocator&)            = delete;
    XYArenaAllocator& operator=(const XYArenaAllocator&) = delete;

    void* alloc(size_t size, size_t alignment = 16);

    template<typename T>
    T* alloc_one()      { return static_cast<T*>(alloc(sizeof(T), alignof(T))); }

    template<typename T>
    T* alloc_array(size_t n) { return static_cast<T*>(alloc(sizeof(T)*n, alignof(T))); }

    void  reset();

    // Save/restore positions for sub-regions.
    size_t save()  const { return offset_; }
    void   restore(size_t mark);

    size_t used()      const { return offset_; }
    size_t capacity()  const { return capacity_; }
    size_t remaining() const { return capacity_ - offset_; }

private:
    uint8_t* base_;
    size_t   capacity_;
    size_t   offset_;
    bool     owns_buffer_;
};

// =============================================================================
// XYStackAllocator
// =============================================================================
//  Stack (LIFO) allocator with explicit markers.  Ideal for temporary
//  per-frame data or nested scopes.  Each alloc stores a small header
//  so you can pop back to any marker without remembering sizes yourself.
//
//  Typical use: per-frame scratch, geometry batches, render lists.
// =============================================================================

class XYStackAllocator {
public:
    explicit XYStackAllocator(size_t capacity = XY_FRAME_STACK_SIZE);
    XYStackAllocator(void* buffer, size_t capacity);
    ~XYStackAllocator();

    XYStackAllocator(const XYStackAllocator&)            = delete;
    XYStackAllocator& operator=(const XYStackAllocator&) = delete;

    void* alloc(size_t size, size_t alignment = 16);

    template<typename T>
    T* alloc_one() { return static_cast<T*>(alloc(sizeof(T), alignof(T))); }

    // Returns opaque marker; pass to pop() to free everything above it.
    size_t push();
    void   pop(size_t marker);

    // Free everything.
    void reset();

    size_t used()     const { return top_; }
    size_t capacity() const { return capacity_; }

private:
    struct BlockHeader {
        size_t prev_top;    // top before this alloc
        size_t size;
        uint32_t guard;
    };
    static constexpr uint32_t STACK_GUARD = 0x5354434B; // 'STCK'

    uint8_t* base_;
    size_t   capacity_;
    size_t   top_;
    bool     owns_buffer_;
};

// =============================================================================
// XYPoolAllocator
// =============================================================================
//  Fixed-size slab allocator.  Maintains a freelist of N objects of type T.
//  Alloc/free are O(1) and produce no fragmentation.
//
//  Typical use: XYSprite, XYMesh nodes, particles, audio voices.
// =============================================================================

template<typename T, size_t N>
class XYPoolAllocator {
    static_assert(sizeof(T) >= sizeof(void*),
                  "Pool object must be at least pointer-sized");
    static_assert(N > 0, "Pool size must be non-zero");

public:
    XYPoolAllocator() {
        // Build the freelist by chaining objects as raw pointer-sized slots.
        freelist_ = nullptr;
        for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
            Slot* s = reinterpret_cast<Slot*>(&storage_[i]);
            s->next  = freelist_;
            freelist_ = s;
        }
        used_ = 0;
    }

    T* alloc() {
        if (!freelist_) return nullptr;
        Slot* s   = freelist_;
        freelist_ = s->next;
        ++used_;
        return reinterpret_cast<T*>(s);
    }

    void free(T* ptr) {
        if (!ptr) return;
        Slot* s   = reinterpret_cast<Slot*>(ptr);
        s->next   = freelist_;
        freelist_ = s;
        --used_;
    }

    // Construct/destroy helpers.
    template<typename... Args>
    T* create(Args&&... args) {
        T* p = alloc();
        if (p) ::new(p) T(static_cast<Args&&>(args)...);
        return p;
    }

    void destroy(T* p) {
        if (p) { p->~T(); free(p); }
    }

    size_t used()      const { return used_; }
    size_t capacity()  const { return N; }
    size_t available() const { return N - used_; }

private:
    struct Slot { Slot* next; };

    // Aligned storage for N objects of type T.
    alignas(T) uint8_t storage_[N][sizeof(T)];
    Slot*  freelist_;
    size_t used_;
};

// =============================================================================
// XYScratchpadAllocator
// =============================================================================
//  Manages the EE Scratch-Pad RAM (SPR) at 0x70000000, 16 KB.
//  Only one allocator instance should own the SPR at a time.
//  Great for DMA descriptors, GIF packets, and temporary VU data.
// =============================================================================

class XYScratchpadAllocator {
public:
    static XYScratchpadAllocator& instance();

    // Acquire/release the SPR.  Must call acquire() before alloc().
    bool acquire();
    void release();
    bool is_acquired() const { return acquired_; }

    void* alloc(size_t size, size_t alignment = 16);
    void  reset();

    size_t used()      const { return offset_; }
    size_t capacity()  const { return XY_SCRATCH_SIZE; }

private:
    XYScratchpadAllocator();

    static constexpr uintptr_t SPR_BASE = 0x70000000u;

    uint8_t* base_;
    size_t   offset_;
    bool     acquired_;
};

// =============================================================================
// XYTransientAllocator
// =============================================================================
//  Double (or triple) buffered linear allocator for per-frame transient data.
//  At the start of each frame call flip() — previous frame's data is still
//  live for one more frame (safe for GPU reads), then wiped automatically.
//
//  Typical use: dynamic VB/IB uploads, particle verts, debug draw lines.
// =============================================================================

class XYTransientAllocator {
public:
    explicit XYTransientAllocator(size_t slot_size    = XY_FRAME_STACK_SIZE,
                                  int    buffer_count = XY_TRANSIENT_BUFFERS);
    ~XYTransientAllocator();

    XYTransientAllocator(const XYTransientAllocator&)            = delete;
    XYTransientAllocator& operator=(const XYTransientAllocator&) = delete;

    // Advance to next buffer slot.  Call once per frame.
    void flip();

    void* alloc(size_t size, size_t alignment = 16);

    template<typename T>
    T* alloc_one() { return static_cast<T*>(alloc(sizeof(T), alignof(T))); }

    template<typename T>
    T* alloc_array(size_t n) { return static_cast<T*>(alloc(sizeof(T)*n, alignof(T))); }

    size_t used()         const;
    size_t slot_capacity() const { return slot_size_; }
    int    buffer_count()  const { return buffer_count_; }
    int    current_slot()  const { return current_; }

private:
    size_t   slot_size_;
    int      buffer_count_;
    int      current_;
    uint8_t* base_;          // flat block: buffer_count * slot_size
    size_t*  offsets_;       // per-slot current offset
};

// =============================================================================
// XYVramAllocator  (improved)
// =============================================================================
//  Best-fit allocator for GS VRAM (4 MB).
//  Supports coalescing of adjacent free blocks to reduce fragmentation.
//  Alignment is always to a 4 KB page boundary (GS requirement).
// =============================================================================

class XYVramAllocator {
public:
    static XYVramAllocator& instance();

    // Allocate `size` bytes in VRAM, returns GS word address (byte / 4).
    // Returns 0 on failure.
    static uint32_t alloc(GSGLOBAL* gs, uint32_t size);
    static void     free(uint32_t address);
    static void     clear(GSGLOBAL* gs);

    static size_t usedBytes();
    static size_t totalBytes();

    // Diagnostic: number of free/used blocks.
    static size_t blockCount();

private:
    XYVramAllocator() : next_ptr_(0), used_bytes_(0), total_(4u*1024u*1024u) {}

    uint32_t _alloc(GSGLOBAL* gs, uint32_t size);
    void     _free(uint32_t address);
    void     _clear(GSGLOBAL* gs);
    void     _coalesce();

    struct Block {
        uint32_t address;
        uint32_t size;
        bool     free;
    };

    // Fixed-size block table — avoids std::vector heap dependency.
    static constexpr size_t MAX_VRAM_BLOCKS = 128;
    Block    blocks_[MAX_VRAM_BLOCKS];
    size_t   num_blocks_;
    uint32_t next_ptr_;
    size_t   used_bytes_;
    size_t   total_;
};

// =============================================================================
// XYHeapAllocator
// =============================================================================
//  General-purpose EE heap wrapper with 128-byte DMA alignment, magic guard,
//  optional memory tags, and optional leak tracking.
// =============================================================================

class XYHeapAllocator {
public:
    static void* alloc(size_t size, size_t alignment = 128);
    static void* alloc_tagged(size_t size, MemTag tag,
                              const char* file = nullptr, int line = 0,
                              size_t alignment = 128);
    static void  free(void* ptr);

    static size_t usedBytes();
    static size_t usedByTag(MemTag tag);
    static void   dumpStats();

    // Replace global new/delete with XY heap (call once at startup).
    static void install_global();

private:
    XYHeapAllocator() = default;
    static XYHeapAllocator& instance();

    void* _alloc(size_t size, MemTag tag,
                 const char* file, int line, size_t alignment);
    void  _free(void* ptr);

    static constexpr uint32_t MEM_MAGIC   = 0x58594F4Eu; // 'XY'
    static constexpr uint32_t MEM_DEAD    = 0xDEADBEEFu;

    struct MemHeader {
        uint32_t magic;
        size_t   size;
        MemTag   tag;
        uint8_t  _pad[3];
#ifdef XY_MEM_DEBUG
        const char* file;
        int         line;
#endif
    };

    size_t used_bytes_;
    size_t used_by_tag_[static_cast<size_t>(MemTag::_Count)];
};

// =============================================================================
// XYIopAllocator
// =============================================================================
//  Manages memory on the I/O Processor (IOP). 
//  Since IOP has 2MB total, this is critical for sound and IRX modules.
// =============================================================================

class XYIopAllocator {
public:
    static XYIopAllocator& instance();

    // Allocates memory on the IOP. Returns IOP-space address.
    static uint32_t alloc(size_t size);
    static void     free(uint32_t address);

    static size_t usedBytes();
    static size_t totalBytes() { return 2u * 1024u * 1024u; }

private:
    XYIopAllocator() : used_bytes_(0) {}
    
    uint32_t _alloc(size_t size);
    void     _free(uint32_t address);

    struct IopBlock {
        uint32_t address;
        size_t   size;
    };

    std::vector<IopBlock> blocks_;
    size_t used_bytes_;
};

// ─── Global Overrides ───────────────────────────────────────────────────────

#if defined(XY_OVERRIDE_NEW)
void* operator new(size_t size);
void* operator new[](size_t size);
void  operator delete(void* ptr) noexcept;
void  operator delete[](void* ptr) noexcept;

// Tagged versions
void* operator new(size_t size, xy::MemTag tag, const char* file, int line);
void* operator new[](size_t size, xy::MemTag tag, const char* file, int line);

#define xy_new(tag) new(tag, __FILE__, __LINE__)
#else
#define xy_new(tag) new
#endif

// ─── C-style convenience wrappers (backward compat) ─────────────────────────

void* ee_alloc(size_t size);
void  ee_free(void* ptr);

// ─── Global frame allocator instance (extern linkage) ───────────────────────

// A global stack allocator reset every frame.
// Access via xy::frame_alloc() and xy::frame_reset().
XYStackAllocator& get_frame_allocator();
void frame_alloc_reset();

void* frame_alloc(size_t size, size_t alignment = 16);

template<typename T>
T* frame_alloc_one() {
    return static_cast<T*>(frame_alloc(sizeof(T), alignof(T)));
}

template<typename T>
T* frame_alloc_array(size_t n) {
    return static_cast<T*>(frame_alloc(sizeof(T) * n, alignof(T)));
}

} // namespace xy
