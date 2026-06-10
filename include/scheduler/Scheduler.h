#pragma once

#include "core/Types.h"
#include "scheduler/MLFQ.h"
#include <mutex>

class ProcessManager;
class MemoryManager;

/// 调度器：管理后台自动调度线程 + 单步执行
class Scheduler {
public:
    Scheduler(MLFQ& mlfq, ProcessManager& pm, MemoryManager& mm,
              std::mutex* backend_mtx = nullptr);

    ~Scheduler();

    // 禁止拷贝
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // ── 调度命令 ──
    std::string start();
    std::string stop();
    std::string restart();
    std::string step();

    // ── 状态 ──
    bool is_running() const;

    /// 等待后台线程退出
    void wait_for_stop();

private:
    MLFQ&           mlfq_;
    ProcessManager& pm_;
    MemoryManager&  mm_;
    std::mutex*     backend_mtx_ = nullptr;

    std::atomic<bool> running_{false};
    std::thread        worker_;
    bool has_snapshot_ = false;
    std::array<std::deque<uint32_t>, 3> snapshot_queues_;
    std::string                          snapshot_pcb_blob_;  // PCB 序列化快照

    /// 后台线程：跑完所有 tick，服务端实时打印
    void auto_loop();

    /// 执行一次调度 tick，返回日志
    std::string do_tick();
    std::string do_tick_with_level(int& level);
    void        save_snapshot();
};
