#pragma once

#include "process/ProcessManager.h"
#include "memory/MemoryManager.h"
#include "scheduler/MLFQ.h"
#include "scheduler/Scheduler.h"
#include <memory>

/// 每个用户的独立系统状态
struct UserState {
    ProcessManager  pm;
    MemoryManager   mm;
    MLFQ            mlfq;
    std::unique_ptr<Scheduler> scheduler;

    void init_scheduler(std::shared_mutex* mtx) {
        if (scheduler) scheduler.reset();
        scheduler.reset(new Scheduler(mlfq, pm, mm, mtx));
    }
};
