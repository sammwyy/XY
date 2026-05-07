#pragma once

#include <functional>
#include <vector>

#include <tamtypes.h>

namespace xy {

using XYTaskId = u32;
using XYTaskCallback = std::function<void()>;

static const XYTaskId XY_TASK_INVALID = 0;

enum class XYTaskType {
    Once,
    Delay,
    Repeat
};

struct XYTaskInfo {
    XYTaskId id;
    XYTaskType type;
    u32 delayMs;
    u32 intervalMs;
    u32 remainingMs;
    int repeatCount;
    int runCount;
    bool paused;
    bool async;
};

class XYTasks {
public:
    XYTasks();
    ~XYTasks();

    XYTaskId run(const XYTaskCallback& callback);
    XYTaskId runDelay(const XYTaskCallback& callback, u32 delayMs);
    XYTaskId runRepeat(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs = 0, int repeatCount = -1);

    XYTaskId run_delay(const XYTaskCallback& callback, u32 delayMs);
    XYTaskId run_repeat(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs = 0, int repeatCount = -1);

    XYTaskId runAsync(const XYTaskCallback& callback);
    XYTaskId runDelayAsync(const XYTaskCallback& callback, u32 delayMs);
    XYTaskId runRepeatAsync(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs = 0, int repeatCount = -1);

    XYTaskId run_async(const XYTaskCallback& callback);
    XYTaskId run_delay_async(const XYTaskCallback& callback, u32 delayMs);
    XYTaskId run_repeat_async(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs = 0, int repeatCount = -1);

    template <typename T>
    XYTaskId run(T* instance, void (T::*method)()) {
        return run([instance, method]() { (instance->*method)(); });
    }

    template <typename T>
    XYTaskId runDelay(T* instance, void (T::*method)(), u32 delayMs) {
        return runDelay([instance, method]() { (instance->*method)(); }, delayMs);
    }

    template <typename T>
    XYTaskId run_delay(T* instance, void (T::*method)(), u32 delayMs) {
        return runDelay(instance, method, delayMs);
    }

    template <typename T>
    XYTaskId runRepeat(T* instance, void (T::*method)(), u32 intervalMs, u32 delayMs = 0, int repeatCount = -1) {
        return runRepeat([instance, method]() { (instance->*method)(); }, intervalMs, delayMs, repeatCount);
    }

    template <typename T>
    XYTaskId run_repeat(T* instance, void (T::*method)(), u32 intervalMs, u32 delayMs = 0, int repeatCount = -1) {
        return runRepeat(instance, method, intervalMs, delayMs, repeatCount);
    }

    template <typename T>
    XYTaskId runAsync(T* instance, void (T::*method)()) {
        return runAsync([instance, method]() { (instance->*method)(); });
    }

    template <typename T>
    XYTaskId run_async(T* instance, void (T::*method)()) {
        return runAsync(instance, method);
    }

    template <typename T>
    XYTaskId runDelayAsync(T* instance, void (T::*method)(), u32 delayMs) {
        return runDelayAsync([instance, method]() { (instance->*method)(); }, delayMs);
    }

    template <typename T>
    XYTaskId run_delay_async(T* instance, void (T::*method)(), u32 delayMs) {
        return runDelayAsync(instance, method, delayMs);
    }

    template <typename T>
    XYTaskId runRepeatAsync(T* instance, void (T::*method)(), u32 intervalMs, u32 delayMs = 0, int repeatCount = -1) {
        return runRepeatAsync([instance, method]() { (instance->*method)(); }, intervalMs, delayMs, repeatCount);
    }

    template <typename T>
    XYTaskId run_repeat_async(T* instance, void (T::*method)(), u32 intervalMs, u32 delayMs = 0, int repeatCount = -1) {
        return runRepeatAsync(instance, method, intervalMs, delayMs, repeatCount);
    }

    bool cancel(XYTaskId id);
    void cancelAll();
    void cancel_all();

    bool pause(XYTaskId id);
    void pauseAll();
    void pause_all();

    bool resume(XYTaskId id);
    void resumeAll();
    void resume_all();

    std::vector<XYTaskInfo> list() const;
    int count() const;
    void update(float dt);
    void shutdown();

private:
    static const int DEFAULT_STACK_SIZE = 16 * 1024;
    static const int DEFAULT_PRIORITY = 64;

    struct Task {
        XYTaskId id;
        XYTaskType type;
        XYTaskCallback callback;
        u32 delayMs;
        u32 intervalMs;
        float mainRemainingMs;
        volatile u32 remainingMs;
        int repeatCount;
        volatile int runCount;
        volatile bool paused;
        volatile bool canceled;
        volatile bool finished;
        bool async;
        s32 threadId;
        u8* stack;
        int stackSize;
        XYTasks* owner;
    };

    XYTaskId addTask(XYTaskType type, const XYTaskCallback& callback, u32 delayMs, u32 intervalMs,
                     int repeatCount, bool async);
    int findIndex(XYTaskId id) const;
    void compactFinished();
    void compactFinishedLocked();
    XYTaskId nextId();
    bool waitTask(Task* task, u32 milliseconds);
    void runTaskThread(Task* task);
    void lock() const;
    void unlock() const;

    static void taskThreadEntry(void* arg);

    std::vector<Task*> tasks_;
    XYTaskId nextId_;
    bool updating_;
    mutable s32 semaId_;
};

} // namespace xy
