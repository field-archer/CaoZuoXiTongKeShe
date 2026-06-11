#include "memory/MemoryManager.h"
#include "core/Config.h"
#include <sstream>
#include <iomanip>

MemoryManager::MemoryManager() {
    // 初始状态：一整块空闲内存
    MemoryBlock block;
    block.start_addr = 0;
    block.size       = TOTAL_MEMORY_KB;
    block.is_free    = true;
    blocks_.push_back(block);
}

// ── 合并相邻的空闲块 ──
void MemoryManager::merge_adjacent_free() {
    for (auto it = blocks_.begin(); it != blocks_.end(); ) {
        if (!it->is_free) { ++it; continue; }
        // 检查下一个是否也是空闲
        auto next = std::next(it);
        if (next != blocks_.end() && next->is_free) {
            it->size += next->size;
            blocks_.erase(next);
            // 不递增 it，继续检查是否还有相邻空闲块
        } else {
            ++it;
        }
    }
}

/// 合并相邻可用块（空闲+换出），alloc 前调用
static void merge_adjacent_available(std::list<MemoryBlock>& blocks) {
    for (auto it = blocks.begin(); it != blocks.end(); ) {
        bool it_avail = it->is_free || it->swapped_out;
        if (!it_avail) { ++it; continue; }
        auto next = std::next(it);
        if (next != blocks.end() && (next->is_free || next->swapped_out)) {
            it->size += next->size;
            it->is_free = true;
            it->swapped_out = false;
            it->owner_pid = 0;
            it->owner_name.clear();
            blocks.erase(next);
        } else { ++it; }
    }
}

// ── 按当前算法查找可用块（空闲 + 换出均可）──
std::list<MemoryBlock>::iterator MemoryManager::find_free_block(uint32_t size) {
    auto best = blocks_.end();
    auto is_available = [](const MemoryBlock& b) {
        return b.is_free || b.swapped_out;
    };

    switch (algo_) {
    case AllocAlgorithm::FIRST_FIT:
        for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
            if (is_available(*it) && it->size >= size) return it;
        }
        break;

    case AllocAlgorithm::BEST_FIT:
        for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
            if (is_available(*it) && it->size >= size) {
                if (best == blocks_.end() || it->size < best->size) {
                    best = it;
                }
            }
        }
        break;

    case AllocAlgorithm::WORST_FIT:
        for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
            if (is_available(*it) && it->size >= size) {
                if (best == blocks_.end() || it->size > best->size) {
                    best = it;
                }
            }
        }
        break;
    }

    return best;
}

// ── 1. alloc ──
std::string MemoryManager::alloc(uint32_t size, uint32_t pid, const std::string& pname) {
    if (size == 0) return "Error: 分配大小必须大于 0 KB。";

    merge_adjacent_available(blocks_);
    auto it = find_free_block(size);
    if (it == blocks_.end()) {
        return "Error: 内存不足，无法分配 " + std::to_string(size) + "KB。";
    }

    uint32_t alloc_addr = it->start_addr;

    if (it->size == size) {
        // 精确匹配，直接标记为已分配
        it->is_free      = false;
        it->swapped_out  = false;
        it->owner_pid    = pid;
        it->owner_name   = pname;
    } else {
        // 分割：后半部分分配给进程，前半部分保持空闲
        // 或者前 N KB 分配，后面保持空闲
        MemoryBlock alloc_block;
        alloc_block.start_addr  = it->start_addr;
        alloc_block.size        = size;
        alloc_block.is_free     = false;
        alloc_block.swapped_out = false;
        alloc_block.owner_pid   = pid;
        alloc_block.owner_name  = pname;

        it->start_addr += size;
        it->size       -= size;

        // 在 it 前面插入分配块
        blocks_.insert(it, alloc_block);
    }

    std::ostringstream oss;
    oss << "分配成功。起始地址=" << alloc_addr << "KB, 大小=" << size
        << "KB, " << algo_to_string(algo_);
    return oss.str();
}

// ── 2. free_mem ──
std::string MemoryManager::free_mem(uint32_t start_addr) {
    for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
        if (it->start_addr == start_addr && !it->is_free) {
            std::string name = it->owner_name;
            uint32_t size = it->size;
            uint32_t pid = it->owner_pid;

            it->is_free    = true;
            it->owner_pid  = 0;
            it->owner_name = "";
            it->swapped_out = false;

            merge_adjacent_free();

            std::ostringstream oss;
            oss << "释放成功。地址=" << start_addr << "KB, 大小=" << size
                << "KB, 原属=" << name << "(" << pid << ")";
            return oss.str();
        }
    }
    return "Error: 未找到地址 " + std::to_string(start_addr) + "KB 处的已分配内存块。";
}

// ── 3. show_mem ──
std::string MemoryManager::show_mem() const {
    std::ostringstream oss;
    oss << "─────────────── 内存布局 ───────────────" << std::endl;
    oss << "算法: " << algo_to_string(algo_) << std::endl;
    oss << "Memory Map (0-" << TOTAL_MEMORY_KB << "KB):" << std::endl;
    for (const auto& b : blocks_) {
        if (b.is_free) oss << "|--free(" << b.size << "K)--";
        else if (b.swapped_out) oss << "|??" << b.owner_name.substr(0,6) << "(" << b.size << "K)";
        else oss << "|##" << b.owner_name.substr(0,6) << "(" << b.size << "K)";
    }
    oss << "|" << std::endl;
    oss << "─────────────────────────────────────────" << std::endl;
    oss << "起始地址\t大小\t\t状态\t\t所属" << std::endl;
    oss << "─────────────────────────────────────────" << std::endl;
    for (const auto& b : blocks_) {
        oss << b.start_addr << "K\t\t" << b.size << "K\t\t";
        if (b.is_free) oss << "空闲\t\t—";
        else if (b.swapped_out) oss << "已换出\t\t" << b.owner_name << "(" << b.owner_pid << ")";
        else oss << "已分配\t\t" << b.owner_name << "(" << b.owner_pid << ")";
        oss << std::endl;
    }
    oss << "─────────────────────────────────────────";
    return oss.str();
}

// ── 4. compact ──
std::vector<CompactResult> MemoryManager::compact() {
    std::vector<CompactResult> mapping;

    // 收集所有已分配块（非空闲、非换出），按原地址排序
    std::vector<MemoryBlock> allocated;
    for (const auto& b : blocks_)
        if (!b.is_free && !b.swapped_out)
            allocated.push_back(b);

    if (allocated.empty()) {
        // 全是空闲：合并为一块
        blocks_.clear();
        MemoryBlock big_free;
        big_free.start_addr = 0;
        big_free.size = TOTAL_MEMORY_KB;
        big_free.is_free = true;
        blocks_.push_back(big_free);
        return mapping;
    }

    // 将已分配块从地址 0 开始紧密排列
    blocks_.clear();
    uint32_t cursor = 0;
    for (auto& b : allocated) {
        CompactResult cr;
        cr.pid      = b.owner_pid;
        cr.old_addr = b.start_addr;
        cr.new_addr = cursor;
        mapping.push_back(cr);

        b.start_addr = cursor;
        blocks_.push_back(b);  // 已分配块
        cursor += b.size;
    }

    // 剩余空间为一个大的空闲块
    if (cursor < TOTAL_MEMORY_KB) {
        MemoryBlock big_free;
        big_free.start_addr = cursor;
        big_free.size = TOTAL_MEMORY_KB - cursor;
        big_free.is_free = true;
        blocks_.push_back(big_free);
    }

    return mapping;
}

// ── 5. mem_stat ──
std::string MemoryManager::mem_stat() const {
    uint32_t total_free = 0, max_free = 0;
    uint32_t total_alloc = 0, total_swapped = 0;
    int free_count = 0, alloc_count = 0, swapped_count = 0;

    for (const auto& b : blocks_) {
        if (b.is_free) {
            total_free += b.size;
            if (b.size > max_free) max_free = b.size;
            ++free_count;
        } else if (b.swapped_out) {
            total_swapped += b.size;
            ++swapped_count;
        } else {
            total_alloc += b.size;
            ++alloc_count;
        }
    }

    double frag_rate = 0.0;
    if (total_free > 0) {
        frag_rate = (1.0 - static_cast<double>(max_free) / total_free) * 100.0;
    }

    std::ostringstream oss;
    oss << "═══════════════ 内存统计 ═══════════════" << std::endl;
    oss << "  总内存:       " << TOTAL_MEMORY_KB << " KB" << std::endl;
    oss << "  已分配:       " << total_alloc << " KB (" << alloc_count << " 块)" << std::endl;
    oss << "  已换出:       " << total_swapped << " KB (" << swapped_count << " 块)" << std::endl;
    oss << "  空闲:         " << total_free << " KB (" << free_count << " 块)" << std::endl;
    oss << "  最大空闲块:   " << max_free << " KB" << std::endl;
    oss << "  碎片率:       " << std::fixed << std::setprecision(1) << frag_rate << "%" << std::endl;
    oss << "  分配算法:     " << algo_to_string(algo_) << std::endl;
    oss << "══════════════════════════════════════════";
    return oss.str();
}

// ── 6. set_alloc_algo ──
std::string MemoryManager::set_alloc_algo(const std::string& algo_name) {
    AllocAlgorithm new_algo = parse_algo(algo_name);
    // 如果输入不能明确解析，且老算法不变的话，报错
    if (new_algo == algo_ &&
        algo_name != "ff" && algo_name != "FF" &&
        algo_name != "bf" && algo_name != "BF" &&
        algo_name != "wf" && algo_name != "WF") {
        // 尝试再次匹配
        new_algo = parse_algo(algo_name);
    }

    algo_ = new_algo;
    return "分配算法已切换为: " + algo_to_string(algo_);
}

// ── 7. pgfault ──
std::string MemoryManager::pgfault() const {
    return "[缺页中断] 访问的页面不在物理内存中，触发缺页异常处理程序。\n"
           "           操作系统将从交换空间调入所需页面，更新页表项。";
}

// ── 8. swap_out ──
// 标记为换出（不释放），返回换出总大小
std::pair<uint32_t, std::string> MemoryManager::swap_out(uint32_t pid) {
    uint32_t total_size = 0;
    int count = 0;
    for (auto& b : blocks_) {
        if (!b.is_free && b.owner_pid == pid && !b.swapped_out) {
            b.swapped_out = true;
            total_size += b.size;
            ++count;
        }
    }
    if (count == 0)
        return {0, "进程 " + std::to_string(pid) + " 没有可换出的内存块。"};
    std::ostringstream oss;
    oss << "进程 " << pid << " 的 " << count << " 个内存块("
        << total_size << "KB)已换出到磁盘。";
    return {total_size, oss.str()};
}

/// 缺页换入：检查是否有足够空间（本进程换出块 + 空闲），够则换入，否则失败
uint32_t MemoryManager::pgfault_alloc(uint32_t pid, uint32_t needed_size) {
    // 1) 统计本进程换出块总大小 + 空闲总大小
    uint32_t swapped = 0, total_free = 0;
    for (const auto& b : blocks_) {
        if (b.swapped_out) swapped += b.size;
        else if (b.is_free) total_free += b.size;
    }
    if (swapped + total_free < needed_size)
        return UINT32_MAX; // 不够

    // 2) 取消本进程换出标记
    for (auto& b : blocks_)
        if (b.swapped_out)
            b.swapped_out = false;
    merge_adjacent_free();

    // 3) 如果还不够，从空闲区补
    uint32_t now_owned = 0;
    for (auto& b : blocks_)
        if (!b.is_free && !b.swapped_out && b.owner_pid == pid)
            now_owned += b.size;

    if (now_owned < needed_size) {
        uint32_t shortage = needed_size - now_owned;
        auto it = find_free_block(shortage);
        if (it != blocks_.end()) {
            if (it->size == shortage) {
                it->is_free = false;
                it->owner_pid = pid;
            } else {
                MemoryBlock extra;
                extra.start_addr = it->start_addr;
                extra.size = shortage;
                extra.is_free = false;
                extra.owner_pid = pid;
                it->start_addr += shortage;
                it->size -= shortage;
                blocks_.insert(it, extra);
            }
        } else {
            return UINT32_MAX;
        }
    }

    // 4) 返回进程拥有的首个块地址
    for (auto& b : blocks_)
        if (!b.is_free && !b.swapped_out && b.owner_pid == pid)
            return b.start_addr;
    return UINT32_MAX;
}

// ── 批量释放（kill_pcb 时调用）──
void MemoryManager::get_pid_blocks(uint32_t pid, std::vector<uint32_t>& addrs,
                                   std::vector<uint32_t>& sizes) const {
    for (const auto& b : blocks_)
        if (!b.is_free && !b.swapped_out && b.owner_pid == pid) {
            addrs.push_back(b.start_addr);
            sizes.push_back(b.size);
        }
}

void MemoryManager::free_all_by_pid(uint32_t pid) {
    std::vector<uint32_t> addrs_to_free;
    for (const auto& b : blocks_) {
        if (!b.is_free && b.owner_pid == pid) {
            addrs_to_free.push_back(b.start_addr);
        }
    }
    for (uint32_t addr : addrs_to_free) {
        free_mem(addr);
    }
}

// ── 持久化 ──
void MemoryManager::serialize(std::ostream& os) const {
    uint8_t algo_byte = static_cast<uint8_t>(algo_);
    os.write(reinterpret_cast<const char*>(&algo_byte), sizeof(algo_byte));

    uint32_t count = static_cast<uint32_t>(blocks_.size());
    os.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& b : blocks_) {
        os.write(reinterpret_cast<const char*>(&b.start_addr), sizeof(b.start_addr));
        os.write(reinterpret_cast<const char*>(&b.size), sizeof(b.size));
        uint8_t flags = 0;
        if (b.is_free)     flags |= 0x01;
        if (b.swapped_out) flags |= 0x02;
        os.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
        os.write(reinterpret_cast<const char*>(&b.owner_pid), sizeof(b.owner_pid));

        uint16_t name_len = static_cast<uint16_t>(b.owner_name.size());
        os.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        if (name_len > 0) {
            os.write(b.owner_name.data(), name_len);
        }
    }
}

void MemoryManager::deserialize(std::istream& is) {
    blocks_.clear();

    uint8_t algo_byte = 0;
    is.read(reinterpret_cast<char*>(&algo_byte), sizeof(algo_byte));
    algo_ = static_cast<AllocAlgorithm>(algo_byte);

    uint32_t count = 0;
    is.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (is.fail()) return;

    for (uint32_t i = 0; i < count; ++i) {
        MemoryBlock b;
        is.read(reinterpret_cast<char*>(&b.start_addr), sizeof(b.start_addr));
        is.read(reinterpret_cast<char*>(&b.size), sizeof(b.size));

        uint8_t flags = 0;
        is.read(reinterpret_cast<char*>(&flags), sizeof(flags));
        b.is_free     = (flags & 0x01) != 0;
        b.swapped_out = (flags & 0x02) != 0;

        is.read(reinterpret_cast<char*>(&b.owner_pid), sizeof(b.owner_pid));

        uint16_t name_len = 0;
        is.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        if (name_len > 0) {
            b.owner_name.resize(name_len);
            is.read(&b.owner_name[0], name_len);
        }

        blocks_.push_back(b);
    }
}
