#pragma once

#include <cstdint>

// ── 内存配置 ──
constexpr uint32_t TOTAL_MEMORY_KB   = 1024;   // 总内存 1024KB (1MB)

// ── MLFQ 配置 ──
constexpr int      MLFQ_NUM_QUEUES   = 3;

// Q0: 优先级 0-3,  时间片 2
// Q1: 优先级 4-7,  时间片 4
// Q2: 优先级 8-15, 时间片 8
struct MLFQConfig {
    int      level;
    int      prio_min;
    int      prio_max;
    uint32_t time_slice_ticks;
};

constexpr MLFQConfig MLFQ_LEVELS[] = {
    { 0, 0,  3,  2 },
    { 1, 4,  7,  4 },
    { 2, 8, 15,  8 },
};

// ── 调度器配置 ──
constexpr uint32_t TICK_DURATION_MS = 500;      // step 默认间隔
constexpr uint32_t SCHED_POLL_INTERVAL_MS = 200;
// auto_loop 实际等待时间（Q0/Q1/Q2），但 tick 扣除量仍为 MLFQ_LEVELS 的 2/4/8
constexpr uint32_t WAIT_Q0_MS = 1000;
constexpr uint32_t WAIT_Q1_MS = 2000;
constexpr uint32_t WAIT_Q2_MS = 3000;

// ── 用户管理配置 ──
constexpr int      MAX_LOGIN_ATTEMPTS = 3;       // 密码错误锁定阈值

// ── 持久化配置 ──
constexpr const char* DEFAULT_STATE_FILE = "os_state.bin";

