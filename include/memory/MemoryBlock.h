#pragma once

#include "core/Types.h"

/// 内存块描述符（空闲或已分配）
struct MemoryBlock {
    uint32_t    start_addr  = 0;     // 起始地址 (KB)
    uint32_t    size        = 0;     // 大小 (KB)
    bool        is_free     = true;
    uint32_t    owner_pid   = 0;     // 所属进程 PID（0 = 空闲）
    std::string owner_name;          // 进程名（便于显示）
    bool        swapped_out = false; // 是否已换出
};

/// compact 返回的地址映射
struct CompactResult {
    uint32_t pid;
    uint32_t old_addr;
    uint32_t new_addr;
};
