#include "scheduler/Scheduler.h"
#include "process/ProcessManager.h"
#include "memory/MemoryManager.h"
#include "core/Platform.h"
#include "core/Config.h"
#include <sstream>

Scheduler::Scheduler(MLFQ& mlfq, ProcessManager& pm, MemoryManager& mm,
                     std::mutex* backend_mtx)
    : mlfq_(mlfq), pm_(pm), mm_(mm), backend_mtx_(backend_mtx) {}

Scheduler::~Scheduler() {
    stop();
    wait_for_stop();
}

bool Scheduler::is_running() const { return running_.load(); }

std::string Scheduler::start() {
    if (running_.load()) return "Error: 调度器已在运行中。";
    wait_for_stop();
    save_snapshot();
    running_.store(true);
    worker_ = std::thread(&Scheduler::auto_loop, this);
    return "调度器已启动。";
}

std::string Scheduler::stop() {
    if (!running_.load()) return "Error: 调度器未在运行。";
    running_.store(false);
    return "调度器已暂停。";
}

std::string Scheduler::restart() {
    if (!has_snapshot_) return "Error: 尚未 start 或 step，无快照可恢复。";
    stop();
    wait_for_stop();
    wait_for_stop(); // 确保旧线程清干净
    mlfq_.clear();
    // 恢复 PCB 状态
    std::istringstream iss(snapshot_pcb_blob_, std::ios::binary);
    pm_.deserialize(iss);
    // 恢复队列
    for (int i = 0; i < 3; ++i)
        for (auto pid : snapshot_queues_[i])
            mlfq_.enqueue_to_level(pid, i);
    running_.store(true);
    worker_ = std::thread(&Scheduler::auto_loop, this);
    return "调度器已从快照重启。";
}

std::string Scheduler::step() {
    if (running_.load()) return "Error: 调度器正在自动运行，请先 stop 再使用 step。";
    save_snapshot();
    return "═══════ Step ═══════\n" + do_tick();
}

void Scheduler::wait_for_stop() {
    if (worker_.joinable()) worker_.join();
}

void Scheduler::save_snapshot() {
    for (int i = 0; i < 3; ++i) snapshot_queues_[i] = mlfq_.queue(i);
    std::ostringstream oss(std::ios::binary);
    pm_.serialize(oss);
    snapshot_pcb_blob_ = oss.str();
    has_snapshot_ = true;
}

// ── 后台线程：实时打印，真等待 ──
void Scheduler::auto_loop() {
    std::cout << "══════════ 调度开始 ══════════" << std::endl;
    while (running_.load()) {
        bool all_empty = true;
        for (int i = 0; i < 3; ++i)
            if (!mlfq_.queue(i).empty()) { all_empty = false; break; }
        if (all_empty) {
            std::cout << "[调度] 所有队列为空，调度器自动停止。" << std::endl;
            std::cout << "══════════ 调度结束 ══════════" << std::endl;
            running_.store(false);
            break;
        }

        int tick_level = 0;
        std::string log;
        if (backend_mtx_) backend_mtx_->lock();
        log = do_tick_with_level(tick_level);
        if (backend_mtx_) backend_mtx_->unlock();

        std::cout << log << std::endl;

        // Q0等1s, Q1等2s, Q2等3s（实际扣2/4/8）
        if (running_.load()) {
            uint32_t wait_ms = 1000;
            if (tick_level == 1) wait_ms = 2000;
            else if (tick_level == 2) wait_ms = 3000;
            for (int i = 0; i < 10 && running_.load(); ++i)
                Platform::sleep_ms(wait_ms / 10);
        }
    }
}

std::string Scheduler::do_tick() {
    int unused;
    return do_tick_with_level(unused);
}

// ── 核心 tick（返回处理的队列层级）──
std::string Scheduler::do_tick_with_level(int& out_level) {
    int level = -1;
    for (int attempt = 0; attempt < 3; ++attempt)
        if (!mlfq_.queue(attempt).empty()) { level = attempt; break; }

    if (level < 0)
        return "[调度] 所有调度队列为空。";
    out_level = level;

    std::string queue_snapshot = mlfq_.snapshot(&pm_);

    uint32_t pid = mlfq_.dequeue_from(level);
    PCB* pcb = pm_.get_pcb(pid);
    if (!pcb) return "[调度] PID " + std::to_string(pid) + " 无效。";

    uint32_t time_slice = MLFQ_LEVELS[level].time_slice_ticks;
    uint32_t remaining  = pcb->remaining_time();
    uint32_t run_ticks  = std::min(time_slice, remaining);

    // pgfault
    bool had_pgfault = false;
    if (pcb->swapped_out && pcb->swapped_size > 0) {
        had_pgfault = true;
        uint32_t new_addr = mm_.pgfault_alloc(pid, pcb->swapped_size);
        if (new_addr != UINT32_MAX) {
            pcb->swapped_out = false;
            pcb->mem_block_addrs.clear();
            pcb->mem_block_sizes.clear();
            mm_.get_pid_blocks(pid, pcb->mem_block_addrs, pcb->mem_block_sizes);
        } else {
            pcb->state = ProcessState::BLOCKED;
            std::ostringstream pglog;
            pglog << "[pgfault] " << pcb->name << "(" << pid
                  << ") 缺页: 内存不足(" << pcb->swapped_size << "KB)，阻塞。";
            return pglog.str();
        }
    }

    std::ostringstream log;
    log << "[队列快照]" << std::endl << queue_snapshot << std::endl << std::endl;
    log << "[决策逻辑]" << std::endl;
    for (int q = 0; q < level; q++) log << " Q" << q << " 为空" << std::endl;
    if (had_pgfault) {
        log << " [缺页中断] 从磁盘换入 " << pcb->swapped_size << "KB 内存" << std::endl;
    }
    log << " 执行Q" << level << "的 "
        << pcb->name << "(" << pcb->pid << "," << remaining << "s) "
        << time_slice << "s" << std::endl;

    pcb->total_cpu_ticks += run_ticks;

    log << "[执行结果]" << std::endl;
    if (pcb->is_finished()) {
        log << " " << pcb->name << "(" << pcb->pid << ") 时间耗尽 → 终止" << std::endl;
        mm_.free_all_by_pid(pid);
        pcb->state = ProcessState::TERMINATED;
        mlfq_.remove(pid);
    } else if (run_ticks >= time_slice) {
        if (level < 2) {
            log << " " << pcb->name << "(" << pcb->pid
                << "," << pcb->remaining_time() << "s)"
                << " → 降级Q" << (level + 1) << std::endl;
            mlfq_.enqueue_to_level(pid, level + 1);
            pcb->queue_level = static_cast<int8_t>(level + 1);
        } else {
            log << " " << pcb->name << "(" << pcb->pid
                << "," << pcb->remaining_time() << "s)"
                << " → 留在Q2队尾" << std::endl;
            mlfq_.enqueue_to_level(pid, 2);
        }
    }
    return log.str();
}
