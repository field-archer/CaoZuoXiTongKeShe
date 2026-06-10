#pragma once

#include "core/Types.h"

/// 进程控制块 (Process Control Block)
struct PCB {
    uint32_t    pid              = 0;
    std::string name;
    ProcessState state           = ProcessState::NEW;
    int8_t      priority         = 0;
    uint32_t    parent_pid       = 1;     // 父进程 PID，默认 init
    std::string owner;                     // 所属用户名
    uint64_t    creation_time    = 0;

    // 父子关系
    std::vector<uint32_t> children_pids;  // kill 时递归

    // 调度
    uint32_t    required_time    = 0;
    uint32_t    total_cpu_ticks  = 0;
    int8_t      queue_level      = 0;
    bool        suspended        = false;

    // 内存跟踪
    std::vector<uint32_t> mem_block_addrs;
    std::vector<uint32_t> mem_block_sizes;
    bool        swapped_out     = false;
    uint32_t    swapped_size    = 0;      // 换出前总内存大小，pgfault 时重新 alloc

    uint32_t remaining_time() const {
        return (required_time > total_cpu_ticks) ? (required_time - total_cpu_ticks) : 0;
    }

    bool is_finished() const {
        return total_cpu_ticks >= required_time;
    }
};
