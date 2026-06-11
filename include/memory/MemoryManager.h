#pragma once

#include "core/Types.h"
#include "memory/MemoryBlock.h"
#include <list>

class ProcessManager;

/// 动态分区内存管理器
class MemoryManager {
public:
    MemoryManager();

    // ── 8 个内存命令 ──
    std::string alloc(uint32_t size, uint32_t pid, const std::string& pname);
    std::string free_mem(uint32_t start_addr);
    std::string show_mem() const;
    std::vector<CompactResult> compact();
    std::string mem_stat() const;
    std::string set_alloc_algo(const std::string& algo_name);
    std::string pgfault() const;
    std::pair<uint32_t, std::string> swap_out(uint32_t pid);
    uint32_t pgfault_alloc(uint32_t pid, uint32_t needed_size);

    // ── 批量释放（kill_pcb 时调用）──
    void free_all_by_pid(uint32_t pid);

    // ── 持久化 ──
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);

    // ── 查询 ──
    AllocAlgorithm get_algo() const { return algo_; }
    void get_pid_blocks(uint32_t pid, std::vector<uint32_t>& addrs,
                        std::vector<uint32_t>& sizes) const;

private:
    std::list<MemoryBlock> blocks_;   // 按 start_addr 排序
    AllocAlgorithm         algo_ = AllocAlgorithm::FIRST_FIT;

    /// 合并相邻的空闲块
    void merge_adjacent_free();

    /// 按当前算法选择合适空闲块（返回迭代器）
    std::list<MemoryBlock>::iterator find_free_block(uint32_t size);
};
