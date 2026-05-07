#include "xy_async.hpp"

#include <cstring>
#include <delaythread.h>
#include <kernel.h>

namespace xy {

XYTasks::XYTasks()
    : tasks_(), nextId_(1), updating_(false), semaId_(-1) {
    ee_sema_t sema;
    std::memset(&sema, 0, sizeof(sema));
    sema.init_count = 1;
    sema.max_count = 1;
    semaId_ = CreateSema(&sema);
}

XYTasks::~XYTasks() {
    shutdown();
}

XYTaskId XYTasks::run(const XYTaskCallback& callback) {
    return addTask(XYTaskType::Once, callback, 0, 0, 1, false);
}

XYTaskId XYTasks::runDelay(const XYTaskCallback& callback, u32 delayMs) {
    return addTask(XYTaskType::Delay, callback, delayMs, 0, 1, false);
}

XYTaskId XYTasks::runRepeat(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs, int repeatCount) {
    if (intervalMs == 0) {
        intervalMs = 1;
    }
    return addTask(XYTaskType::Repeat, callback, delayMs, intervalMs, repeatCount, false);
}

XYTaskId XYTasks::run_delay(const XYTaskCallback& callback, u32 delayMs) {
    return runDelay(callback, delayMs);
}

XYTaskId XYTasks::run_repeat(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs, int repeatCount) {
    return runRepeat(callback, intervalMs, delayMs, repeatCount);
}

XYTaskId XYTasks::runAsync(const XYTaskCallback& callback) {
    return addTask(XYTaskType::Once, callback, 0, 0, 1, true);
}

XYTaskId XYTasks::runDelayAsync(const XYTaskCallback& callback, u32 delayMs) {
    return addTask(XYTaskType::Delay, callback, delayMs, 0, 1, true);
}

XYTaskId XYTasks::runRepeatAsync(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs, int repeatCount) {
    if (intervalMs == 0) {
        intervalMs = 1;
    }
    return addTask(XYTaskType::Repeat, callback, delayMs, intervalMs, repeatCount, true);
}

XYTaskId XYTasks::run_async(const XYTaskCallback& callback) {
    return runAsync(callback);
}

XYTaskId XYTasks::run_delay_async(const XYTaskCallback& callback, u32 delayMs) {
    return runDelayAsync(callback, delayMs);
}

XYTaskId XYTasks::run_repeat_async(const XYTaskCallback& callback, u32 intervalMs, u32 delayMs, int repeatCount) {
    return runRepeatAsync(callback, intervalMs, delayMs, repeatCount);
}

bool XYTasks::cancel(XYTaskId id) {
    lock();
    int index = findIndex(id);
    if (index < 0) {
        unlock();
        return false;
    }

    tasks_[index]->canceled = true;
    if (!updating_) {
        compactFinishedLocked();
    }
    unlock();
    return true;
}

void XYTasks::cancelAll() {
    lock();
    for (Task* task : tasks_) {
        task->canceled = true;
    }
    if (!updating_) {
        compactFinishedLocked();
    }
    unlock();
}

void XYTasks::cancel_all() {
    cancelAll();
}

bool XYTasks::pause(XYTaskId id) {
    lock();
    int index = findIndex(id);
    if (index < 0) {
        unlock();
        return false;
    }

    tasks_[index]->paused = true;
    unlock();
    return true;
}

void XYTasks::pauseAll() {
    lock();
    for (Task* task : tasks_) {
        task->paused = true;
    }
    unlock();
}

void XYTasks::pause_all() {
    pauseAll();
}

bool XYTasks::resume(XYTaskId id) {
    lock();
    int index = findIndex(id);
    if (index < 0) {
        unlock();
        return false;
    }

    tasks_[index]->paused = false;
    unlock();
    return true;
}

void XYTasks::resumeAll() {
    lock();
    for (Task* task : tasks_) {
        task->paused = false;
    }
    unlock();
}

void XYTasks::resume_all() {
    resumeAll();
}

std::vector<XYTaskInfo> XYTasks::list() const {
    std::vector<XYTaskInfo> info;

    lock();
    info.reserve(tasks_.size());
    for (const Task* task : tasks_) {
        if (task->canceled || task->finished) {
            continue;
        }

        XYTaskInfo item;
        item.id = task->id;
        item.type = task->type;
        item.delayMs = task->delayMs;
        item.intervalMs = task->intervalMs;
        item.remainingMs = task->remainingMs;
        item.repeatCount = task->repeatCount;
        item.runCount = task->runCount;
        item.paused = task->paused;
        item.async = task->async;
        info.push_back(item);
    }
    unlock();

    return info;
}

int XYTasks::count() const {
    int total = 0;

    lock();
    for (const Task* task : tasks_) {
        if (!task->canceled && !task->finished) {
            ++total;
        }
    }
    unlock();

    return total;
}

void XYTasks::update(float dt) {
    if (dt < 0.0f) {
        dt = 0.0f;
    }

    const float deltaMs = dt * 1000.0f;

    lock();
    const int initialCount = static_cast<int>(tasks_.size());
    updating_ = true;
    unlock();

    for (int i = 0; i < initialCount; ++i) {
        lock();
        if (i >= static_cast<int>(tasks_.size())) {
            unlock();
            break;
        }

        Task* task = tasks_[i];
        if (task->async || task->canceled || task->finished || task->paused) {
            unlock();
            continue;
        }

        task->mainRemainingMs -= deltaMs;
        task->remainingMs = task->mainRemainingMs > 0.0f ? static_cast<u32>(task->mainRemainingMs + 0.5f) : 0;
        if (task->mainRemainingMs > 0.0f) {
            unlock();
            continue;
        }

        const XYTaskId id = task->id;
        const XYTaskCallback callback = task->callback;
        unlock();

        if (callback) {
            callback();
        }

        lock();
        const int index = findIndex(id);
        if (index >= 0) {
            Task* updated = tasks_[index];
            if (!updated->canceled && !updated->finished) {
                ++updated->runCount;

                if (updated->type == XYTaskType::Repeat &&
                    (updated->repeatCount < 0 || updated->runCount < updated->repeatCount)) {
                    updated->mainRemainingMs += static_cast<float>(updated->intervalMs);
                    if (updated->mainRemainingMs <= 0.0f) {
                        updated->mainRemainingMs = static_cast<float>(updated->intervalMs);
                    }
                    updated->remainingMs = static_cast<u32>(updated->mainRemainingMs + 0.5f);
                } else {
                    updated->canceled = true;
                }
            }
        }
        unlock();
    }

    lock();
    updating_ = false;
    compactFinishedLocked();
    unlock();
}

void XYTasks::shutdown() {
    lock();
    for (Task* task : tasks_) {
        task->canceled = true;
    }
    unlock();

    for (;;) {
        lock();
        if (tasks_.empty()) {
            unlock();
            break;
        }

        Task* task = tasks_.back();
        tasks_.pop_back();
        unlock();

        if (task->async && task->threadId >= 0) {
            if (!task->finished) {
                TerminateThread(task->threadId);
            }
            DeleteThread(task->threadId);
        }

        delete[] task->stack;
        delete task;
    }

    if (semaId_ >= 0) {
        DeleteSema(semaId_);
        semaId_ = -1;
    }
}

XYTaskId XYTasks::addTask(XYTaskType type, const XYTaskCallback& callback, u32 delayMs, u32 intervalMs,
                          int repeatCount, bool async) {
    if (!callback) {
        return XY_TASK_INVALID;
    }

    Task* task = new Task();
    lock();
    task->id = nextId();
    unlock();
    task->type = type;
    task->callback = callback;
    task->delayMs = delayMs;
    task->intervalMs = intervalMs;
    task->mainRemainingMs = type == XYTaskType::Repeat && delayMs == 0 ? static_cast<float>(intervalMs) : static_cast<float>(delayMs);
    task->remainingMs = static_cast<u32>(task->mainRemainingMs + 0.5f);
    task->repeatCount = repeatCount;
    task->runCount = 0;
    task->paused = false;
    task->canceled = false;
    task->finished = false;
    task->async = async;
    task->threadId = -1;
    task->stack = nullptr;
    task->stackSize = 0;
    task->owner = this;

    if (!async) {
        lock();
        tasks_.push_back(task);
        unlock();
        return task->id;
    }

    task->stackSize = DEFAULT_STACK_SIZE;
    task->stack = new u8[task->stackSize];
    if (!task->stack) {
        delete task;
        return XY_TASK_INVALID;
    }

    ee_thread_t thread;
    std::memset(&thread, 0, sizeof(thread));
    thread.func = reinterpret_cast<void*>(taskThreadEntry);
    thread.stack = task->stack;
    thread.stack_size = task->stackSize;
    thread.gp_reg = &_gp;
    thread.initial_priority = DEFAULT_PRIORITY;

    task->threadId = CreateThread(&thread);
    if (task->threadId < 0) {
        delete[] task->stack;
        delete task;
        return XY_TASK_INVALID;
    }

    lock();
    tasks_.push_back(task);
    unlock();

    if (StartThread(task->threadId, task) < 0) {
        lock();
        task->finished = true;
        task->canceled = true;
        compactFinishedLocked();
        unlock();
        return XY_TASK_INVALID;
    }

    return task->id;
}

int XYTasks::findIndex(XYTaskId id) const {
    if (id == XY_TASK_INVALID) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(tasks_.size()); ++i) {
        if (tasks_[i]->id == id) {
            return i;
        }
    }
    return -1;
}

void XYTasks::compactFinished() {
    lock();
    compactFinishedLocked();
    unlock();
}

void XYTasks::compactFinishedLocked() {
    for (int i = static_cast<int>(tasks_.size()) - 1; i >= 0; --i) {
        Task* task = tasks_[i];
        const bool removeMain = !task->async && task->canceled && !updating_;
        const bool removeAsync = task->async && task->finished;

        if (!removeMain && !removeAsync) {
            continue;
        }

        tasks_.erase(tasks_.begin() + i);

        if (task->async && task->threadId >= 0) {
            DeleteThread(task->threadId);
        }

        delete[] task->stack;
        delete task;
    }
}

XYTaskId XYTasks::nextId() {
    XYTaskId id = nextId_;
    ++nextId_;
    if (nextId_ == XY_TASK_INVALID) {
        nextId_ = 1;
    }
    return id;
}

bool XYTasks::waitTask(Task* task, u32 milliseconds) {
    u32 remaining = milliseconds;
    task->remainingMs = remaining;

    while (!task->canceled && remaining > 0) {
        if (task->paused) {
            DelayThread(1000);
            continue;
        }

        const u32 step = remaining > 10 ? 10 : remaining;
        DelayThread(static_cast<s32>(step * 1000));

        if (!task->paused) {
            remaining = remaining > step ? remaining - step : 0;
            task->remainingMs = remaining;
        }
    }

    return !task->canceled;
}

void XYTasks::runTaskThread(Task* task) {
    bool firstRun = true;

    while (!task->canceled) {
        u32 waitMs = task->delayMs;
        if (task->type == XYTaskType::Repeat && (!firstRun || waitMs == 0)) {
            waitMs = task->intervalMs;
        }

        if (!waitTask(task, waitMs)) {
            break;
        }

        if (!task->canceled && task->callback) {
            task->callback();
        }

        if (task->canceled) {
            break;
        }

        ++task->runCount;
        if (task->type != XYTaskType::Repeat ||
            (task->repeatCount >= 0 && task->runCount >= task->repeatCount)) {
            break;
        }

        firstRun = false;
    }

    task->remainingMs = 0;
    task->canceled = true;
    task->finished = true;
}

void XYTasks::lock() const {
    if (semaId_ >= 0) {
        WaitSema(semaId_);
    }
}

void XYTasks::unlock() const {
    if (semaId_ >= 0) {
        SignalSema(semaId_);
    }
}

void XYTasks::taskThreadEntry(void* arg) {
    Task* task = static_cast<Task*>(arg);
    task->owner->runTaskThread(task);
    ExitThread();
}

} // namespace xy
