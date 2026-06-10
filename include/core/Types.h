#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <optional>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <chrono>
#include <atomic>
#include <memory>

// ── 进程状态 ──
enum class ProcessState : uint8_t {
    NEW        = 0,
    READY      = 1,
    RUNNING    = 2,
    BLOCKED    = 3,
    SUSPENDED  = 4,
    TERMINATED = 5
};

// ── 内存分配算法 ──
enum class AllocAlgorithm : uint8_t {
    FIRST_FIT  = 0,
    BEST_FIT   = 1,
    WORST_FIT  = 2
};

// ── 消息类型（全部命令）──
enum class MessageType : uint8_t {
    // 用户管理
    REGISTER,
    LOGIN,
    LOGOUT,
    // 进程管理（10个）
    CREATE_PCB,
    KILL_PCB,
    BLOCK_PCB,
    WAKEUP_PCB,
    SHOW_PCB,
    LIST_PCB,
    PTREE,
    SUSPEND,
    RESUME,
    RENICE,
    // 调度器（4个）
    START_SCHED,
    STOP_SCHED,
    RESTART_SCHED,
    STEP,
    // 内存管理（8个）
    ALLOC,
    FREE_MEM,
    SHOW_MEM,
    COMPACT,
    MEM_STAT,
    SET_ALLOC_ALGO,
    PGFAULT,
    SWAP_OUT,
    // 持久化
    SAVE,
    LOAD,
    // 可视化
    OVERVIEW,
    // 控制
    HELP,
    EXIT,
    INVALID
};

// ── 常用工具 ──
inline std::string state_to_string(ProcessState s) {
    switch (s) {
        case ProcessState::NEW:        return "NEW";
        case ProcessState::READY:      return "READY";
        case ProcessState::RUNNING:    return "RUNNING";
        case ProcessState::BLOCKED:    return "BLOCKED";
        case ProcessState::SUSPENDED:  return "SUSPEND";
        case ProcessState::TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

inline std::string algo_to_string(AllocAlgorithm a) {
    switch (a) {
        case AllocAlgorithm::FIRST_FIT: return "First Fit";
        case AllocAlgorithm::BEST_FIT:  return "Best Fit";
        case AllocAlgorithm::WORST_FIT: return "Worst Fit";
        default: return "Unknown";
    }
}

inline AllocAlgorithm parse_algo(const std::string& s) {
    if (s == "ff" || s == "FF" || s == "first_fit") return AllocAlgorithm::FIRST_FIT;
    if (s == "bf" || s == "BF" || s == "best_fit")  return AllocAlgorithm::BEST_FIT;
    if (s == "wf" || s == "WF" || s == "worst_fit") return AllocAlgorithm::WORST_FIT;
    return AllocAlgorithm::FIRST_FIT; // 默认
}
