#include "xy_game.hpp"
#include "xy_math.hpp"
#include <cstdio>
#include <cstring>

// We include the allocator header.
// In a real project, XY_MEM_DEBUG would be in your Makefile/Build system.
#define XY_MEM_DEBUG
#include "xy_alloc.hpp"

using namespace xy;

// A dummy object for our Pool
struct Bullet {
    Vec3 pos;
    Vec3 vel;
    float ttl;
    bool active;
};

class MemoryExample : public XYGame {
public:
    bool onInit() override {
        // [1] Arena for Level Resources
        // We simulate loading a "level" into an arena.
        levelArena_ = new XYArenaAllocator(1024 * 64); // 64KB
        
        // [2] Pool for dynamic objects
        bulletPool_ = new XYPoolAllocator<Bullet, 50>();
        
        // [3] Transient Memory for dynamic geo
        transient_ = new XYTransientAllocator(16 * 1024, 2); // 16KB per frame

        printf("Memory Example Initialized\n");
        return true;
    }

    void onUpdate(float dt) override {
        (void)dt;
        
        // [4] Frame Allocator (Transient EE memory)
        // Reset every frame by the engine (hypothetically, if we call frame_alloc_reset)
        // For this demo, let's just use it.
        char* frameMsg = (char*)frame_alloc(64);
        snprintf(frameMsg, 64, "Frame count: %d", (int)(1.0f/dt));

        // [5] Pool Usage
        if (input().pressed(0, XY_BUTTON_CROSS)) {
            Bullet* b = bulletPool_->alloc();
            if (b) {
                b->active = true;
                b->ttl = 2.0f;
            }
        }
        
        // [6] Scratchpad Usage (for intensive math/DMA)
        auto& spr = XYScratchpadAllocator::instance();
        if (spr.acquire()) {
            void* workBuffer = spr.alloc(1024);
            (void)workBuffer; // Do high-speed work here...
            spr.release();
        }

        // [7] Transient (Double Buffered)
        transient_->flip();
        void* drawCommands = transient_->alloc(128);
        (void)drawCommands;
    }

    void onRender() override {
        graphics().drawText(40, 40, "XY - MEMORY SYSTEM", Color(255, 240, 90), 3.0f);

        // Display Statistics
        graphics().drawFormat(40, 100, Color(160, 255, 190), 2.0f, 
            "EE HEAP USED: %u KB", (unsigned)(XYHeapAllocator::usedBytes() / 1024));
        
        graphics().drawFormat(40, 130, Color(180, 210, 255), 2.0f, 
            "ARENA USED:   %u / %u bytes", (unsigned)levelArena_->used(), (unsigned)levelArena_->capacity());

        graphics().drawFormat(40, 160, Color(180, 210, 255), 2.0f, 
            "POOL USED:    %u / %u objects", (unsigned)bulletPool_->used(), (unsigned)bulletPool_->capacity());

        graphics().drawFormat(40, 190, Color(255, 180, 180), 2.0f, 
            "VRAM USED:    %u / %u KB", (unsigned)(XYVramAllocator::usedBytes() / 1024), (unsigned)(XYVramAllocator::totalBytes() / 1024));

        graphics().drawFormat(40, 220, Color(255, 255, 255), 2.0f, 
            "TRANSIENT:    %u bytes (Slot %d)", (unsigned)transient_->used(), transient_->current_slot());

        graphics().drawText(40, 300, "PRESS CROSS TO ALLOC IN POOL", Color(200, 200, 200), 2.0f);
        graphics().drawText(40, 330, "CHECK SERIAL LOG FOR LEAK REPORT", Color(255, 100, 100), 2.0f);
    }

    void onShutdown() override {
        delete levelArena_;
        delete bulletPool_;
        delete transient_;
        
        // Print final leak report to serial
        XY_MEM_DUMP_LEAKS();
    }

private:
    XYArenaAllocator*     levelArena_ = nullptr;
    XYPoolAllocator<Bullet, 50>* bulletPool_ = nullptr;
    XYTransientAllocator* transient_ = nullptr;
};

int main() {
    MemoryExample game;
    return game.run();
}
