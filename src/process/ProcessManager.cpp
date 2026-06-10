#include "process/ProcessManager.h"
#include "core/Config.h"
#include "core/Platform.h"
#include <sstream>
#include <algorithm>

ProcessManager::ProcessManager() {
    PCB init;
    init.pid           = next_pid_++;
    init.name          = "init";
    init.state         = ProcessState::RUNNING;
    init.priority      = 0;
    init.creation_time = Platform::current_time_ms();
    init.required_time = 0;
    pcbs_.push_back(init);
}

int ProcessManager::find_index(uint32_t pid) const {
    for (size_t i = 0; i < pcbs_.size(); ++i)
        if (pcbs_[i].pid == pid) return static_cast<int>(i);
    return -1;
}

PCB* ProcessManager::get_pcb(uint32_t pid) {
    int idx = find_index(pid);
    return (idx >= 0) ? &pcbs_[idx] : nullptr;
}

const PCB* ProcessManager::get_pcb(uint32_t pid) const {
    int idx = find_index(pid);
    return (idx >= 0) ? &pcbs_[idx] : nullptr;
}

// ── 1. create_pcb ──
std::string ProcessManager::create_pcb(const std::string& name, uint32_t required_time,
                                       const std::string& owner, int queue_level,
                                       uint32_t parent_pid) {
    if (queue_level < 0 || queue_level > 2)
        return "Error: 队列层级必须是 0/1/2。";
    if (required_time == 0)
        return "Error: 所需时间必须大于 0。";
    if (name.empty())
        return "Error: 进程名不能为空。";

    // 检查父进程是否存在（只检查，不用指针——push_back 后 vector 可能重分配）
    int parent_idx = find_index(parent_pid);
    if (parent_idx < 0 || pcbs_[parent_idx].state == ProcessState::TERMINATED)
        return "Error: 父进程 PID=" + std::to_string(parent_pid) + " 不存在或已终止。";

    for (const auto& p : pcbs_)
        if (p.name == name && p.owner == owner && p.state != ProcessState::TERMINATED)
            return "Error: 已存在同名进程 '" + name + "'。";

    PCB pcb;
    pcb.pid           = next_pid_++;
    pcb.name          = name;
    pcb.owner         = owner;
    pcb.parent_pid    = parent_pid;
    pcb.state         = ProcessState::READY;
    pcb.priority      = static_cast<int8_t>(MLFQ_LEVELS[queue_level].prio_min);
    pcb.required_time = required_time;
    pcb.queue_level   = static_cast<int8_t>(queue_level);
    pcb.creation_time = Platform::current_time_ms();

    uint32_t new_pid = pcb.pid;
    pcbs_.push_back(pcb);

    // push_back 后重新获取父进程指针（vector 可能重分配）
    pcbs_[parent_idx].children_pids.push_back(new_pid);

    std::ostringstream oss;
    oss << name << "(" << new_pid << "," << required_time << "s) 创建成功, 父="
        << pcbs_[parent_idx].name << "(" << parent_pid << ")"
        << ", 进入Q" << queue_level
        << "(时间片" << MLFQ_LEVELS[queue_level].time_slice_ticks << "s)";
    return oss.str();
}

// ── 2. kill_pcb ──
// pid: 要杀的进程，killed_pids: 输出参数，收集所有被杀的 PID（含子孙）
static void kill_recursive(uint32_t pid, std::vector<PCB>& pcbs,
                           std::vector<uint32_t>& killed_pids) {
    if (pid == 1) return;
    auto it = std::find_if(pcbs.begin(), pcbs.end(),
                           [pid](const PCB& p) { return p.pid == pid; });
    if (it == pcbs.end() || it->state == ProcessState::TERMINATED) return;

    // 先递归杀子
    auto kids = it->children_pids;
    for (uint32_t cid : kids)
        kill_recursive(cid, pcbs, killed_pids);

    // 从父进程的子列表中移除
    auto pit = std::find_if(pcbs.begin(), pcbs.end(),
                            [&](const PCB& p) { return p.pid == it->parent_pid; });
    if (pit != pcbs.end()) {
        auto& sibs = pit->children_pids;
        sibs.erase(std::remove(sibs.begin(), sibs.end(), pid), sibs.end());
    }

    killed_pids.push_back(pid);
    it->state = ProcessState::TERMINATED;
    it->children_pids.clear();
}

std::string ProcessManager::kill_pcb(uint32_t pid) {
    if (pid == 1) return "Error: 不能撤销 init。";
    if (find_index(pid) < 0) return "Error: 进程 " + std::to_string(pid) + " 不存在。";

    std::vector<uint32_t> killed;
    PCB* target = get_pcb(pid);
    std::string name = target->name;
    bool had_mem = !target->mem_block_addrs.empty();
    size_t mem_blocks = target->mem_block_addrs.size();

    kill_recursive(pid, pcbs_, killed);

    std::ostringstream oss;
    oss << name << "(" << pid << ") 已撤销";
    if (killed.size() > 1) oss << "（含 " << (killed.size() - 1) << " 个子进程）";
    oss << "。";
    if (had_mem) oss << " " << mem_blocks << " 个内存块待回收。";
    return oss.str();
}

// ── 3. block_pcb ──
std::string ProcessManager::block_pcb(uint32_t pid) {
    if (pid == 1) return "Error: 不能阻塞 init。";
    PCB* pcb = get_pcb(pid);
    if (!pcb) return "Error: 进程 " + std::to_string(pid) + " 不存在。";
    if (pcb->state == ProcessState::TERMINATED) return "Error: 进程已终止。";
    if (pcb->state == ProcessState::BLOCKED) return "Error: 进程已阻塞。";
    pcb->state = ProcessState::BLOCKED;
    return pcb->name + "(" + std::to_string(pid) + ") 已阻塞。";
}

// ── 4. wakeup_pcb ──
std::string ProcessManager::wakeup_pcb(uint32_t pid) {
    if (pid == 1) return "Error: 不能唤醒 init。";
    PCB* pcb = get_pcb(pid);
    if (!pcb) return "Error: 进程 " + std::to_string(pid) + " 不存在。";
    if (pcb->state != ProcessState::BLOCKED) return "Error: 进程不在阻塞状态。";
    pcb->state = ProcessState::READY;
    return pcb->name + "(" + std::to_string(pid) + ") 已唤醒 → READY。";
}

// ── 5. show_pcb ──
std::string ProcessManager::show_pcb(uint32_t pid) const {
    int idx = find_index(pid);
    if (idx < 0) return "Error: 进程 " + std::to_string(pid) + " 不存在。";

    const PCB& p = pcbs_[idx];
    std::ostringstream oss;
    oss << p.name << "(" << p.pid << "," << p.required_time << "s)" << std::endl;
    oss << "  状态:     " << state_to_string(p.state)
        << (p.suspended ? " [已挂起]" : "") << std::endl;
    oss << "  优先级:   " << static_cast<int>(p.priority) << std::endl;
    oss << "  队列:     Q" << static_cast<int>(p.queue_level) << std::endl;
    oss << "  已执行:   " << p.total_cpu_ticks << "s" << std::endl;
    oss << "  剩余:     " << p.remaining_time() << "s" << std::endl;
    oss << "  内存块:   ";
    if (p.mem_block_addrs.empty()) {
        oss << "无";
    } else {
        for (size_t i = 0; i < p.mem_block_addrs.size(); ++i)
            oss << "[" << p.mem_block_addrs[i] << "-"
                << (p.mem_block_addrs[i] + p.mem_block_sizes[i] - 1)
                << "](" << p.mem_block_sizes[i] << "K) ";
    }
    return oss.str();
}

// ── 6. list_pcb ──
std::string ProcessManager::list_pcb(const std::string& owner) const {
    std::ostringstream oss;
    oss << "PID\t名称\t\t状态\t\t队列\t总时间\t剩余" << std::endl;
    oss << "───────────────────────────────────────────────────────" << std::endl;

    int count = 0;
    for (const auto& p : pcbs_) {
        if (p.state == ProcessState::TERMINATED) continue;
        if (!owner.empty() && p.owner != owner && p.pid != 1) continue;
        oss << p.pid << "\t"
            << p.name << "\t\t"
            << state_to_string(p.state)
            << (p.suspended ? "[S]" : "") << "\t"
            << "Q" << static_cast<int>(p.queue_level) << "\t"
            << p.required_time << "s\t"
            << p.remaining_time() << "s"
            << std::endl;
        ++count;
    }
    oss << "───────────────────────────────────────────────────────" << std::endl;
    oss << "合计: " << count << " 个进程。";
    return oss.str();
}

// ── 7. ptree ──
static void ptree_recurse(uint32_t pid, const std::string& prefix, bool is_last,
                          const std::vector<PCB>& pcbs, const std::string& owner,
                          std::ostringstream& oss) {
    auto it = std::find_if(pcbs.begin(), pcbs.end(), [pid](const PCB& p) { return p.pid == pid; });
    if (it == pcbs.end() || it->state == ProcessState::TERMINATED) return;
    if (!owner.empty() && it->owner != owner && pid != 1) return;

    oss << prefix;
    if (is_last && !prefix.empty()) {
        // 找最后一个非空格字符位置，替换为 └
        auto pos = prefix.rfind("|");
        if (pos != std::string::npos) {
            oss << prefix.substr(0, pos) << "`-- ";
        } else {
            oss << "`-- ";
        }
    } else if (!prefix.empty()) {
        oss << "|-- ";
    }
    oss << it->name << "(" << pid << "," << it->remaining_time() << "s) ["
        << state_to_string(it->state) << "]" << std::endl;

    // 收集可见子进程
    std::vector<uint32_t> visible;
    for (auto cid : it->children_pids) {
        auto cit = std::find_if(pcbs.begin(), pcbs.end(), [cid](const PCB& p) { return p.pid == cid; });
        if (cit == pcbs.end() || cit->state == ProcessState::TERMINATED) continue;
        if (!owner.empty() && cit->owner != owner) continue;
        visible.push_back(cid);
    }

    std::string child_prefix = prefix;
    if (!prefix.empty()) {
        // 当前行有 "|-- " 或 "`-- "，子进程缩进加 "|   " 或 "    "
        if (is_last)
            child_prefix += "    ";
        else
            child_prefix += "|   ";
    } else {
        child_prefix = "    ";
    }

    for (size_t i = 0; i < visible.size(); ++i)
        ptree_recurse(visible[i], child_prefix, i == visible.size() - 1, pcbs, owner, oss);
}

std::string ProcessManager::ptree(const std::string& owner) const {
    std::ostringstream oss;
    ptree_recurse(1, "", false, pcbs_, owner, oss);
    return oss.str();
}

// ── 8. suspend ──
std::string ProcessManager::suspend(uint32_t pid) {
    if (pid == 1) return "Error: 不能挂起 init。";
    PCB* pcb = get_pcb(pid);
    if (!pcb) return "Error: 进程 " + std::to_string(pid) + " 不存在。";
    if (pcb->state == ProcessState::TERMINATED) return "Error: 进程已终止。";
    if (pcb->suspended) return "Error: 进程已挂起。";
    pcb->suspended = true;
    if (pcb->state == ProcessState::RUNNING || pcb->state == ProcessState::READY)
        pcb->state = ProcessState::SUSPENDED;
    return pcb->name + "(" + std::to_string(pid) + ") 已挂起。";
}

// ── 9. resume ──
std::string ProcessManager::resume(uint32_t pid) {
    if (pid == 1) return "Error: 不能恢复 init。";
    PCB* pcb = get_pcb(pid);
    if (!pcb) return "Error: 进程 " + std::to_string(pid) + " 不存在。";
    if (!pcb->suspended && pcb->state != ProcessState::SUSPENDED)
        return "Error: 进程未挂起。";
    pcb->suspended = false;
    pcb->state = ProcessState::READY;
    return pcb->name + "(" + std::to_string(pid) + ") 已恢复 → READY。";
}

// ── 10. renice ──
std::string ProcessManager::renice(uint32_t pid, int new_queue) {
    if (pid == 1) return "Error: 不能修改 init。";
    if (new_queue < 0 || new_queue > 2)
        return "Error: 目标队列必须是 0/1/2。";
    PCB* pcb = get_pcb(pid);
    if (!pcb) return "Error: 进程 " + std::to_string(pid) + " 不存在。";
    if (pcb->state == ProcessState::TERMINATED) return "Error: 进程已终止。";

    int old_level = pcb->queue_level;
    pcb->queue_level = static_cast<int8_t>(new_queue);
    pcb->priority = static_cast<int8_t>(MLFQ_LEVELS[new_queue].prio_min);

    std::ostringstream oss;
    oss << pcb->name << "(" << pid << ") Q" << old_level
        << "→Q" << new_queue;
    return oss.str();
}

// ── 持久化 ──
void ProcessManager::serialize(std::ostream& os) const {
    uint32_t count = static_cast<uint32_t>(pcbs_.size());
    os.write(reinterpret_cast<const char*>(&count), sizeof(count));
    os.write(reinterpret_cast<const char*>(&next_pid_), sizeof(next_pid_));

    for (const auto& p : pcbs_) {
        os.write(reinterpret_cast<const char*>(&p.pid), sizeof(p.pid));

        uint16_t name_len = static_cast<uint16_t>(p.name.size());
        os.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        os.write(p.name.data(), name_len);

        uint8_t state_byte = static_cast<uint8_t>(p.state);
        os.write(reinterpret_cast<const char*>(&state_byte), sizeof(state_byte));
        os.write(reinterpret_cast<const char*>(&p.priority), sizeof(p.priority));
        os.write(reinterpret_cast<const char*>(&p.parent_pid), sizeof(p.parent_pid));

        uint16_t owner_len = static_cast<uint16_t>(p.owner.size());
        os.write(reinterpret_cast<const char*>(&owner_len), sizeof(owner_len));
        os.write(p.owner.data(), owner_len);

        os.write(reinterpret_cast<const char*>(&p.creation_time), sizeof(p.creation_time));
        os.write(reinterpret_cast<const char*>(&p.required_time), sizeof(p.required_time));
        os.write(reinterpret_cast<const char*>(&p.total_cpu_ticks), sizeof(p.total_cpu_ticks));
        os.write(reinterpret_cast<const char*>(&p.queue_level), sizeof(p.queue_level));

        uint8_t suspended_byte = p.suspended ? 1 : 0;
        os.write(reinterpret_cast<const char*>(&suspended_byte), sizeof(suspended_byte));

        uint8_t swapped_byte = p.swapped_out ? 1 : 0;
        os.write(reinterpret_cast<const char*>(&swapped_byte), sizeof(swapped_byte));
        os.write(reinterpret_cast<const char*>(&p.swapped_size), sizeof(p.swapped_size));

        // children_pids
        uint32_t child_count = static_cast<uint32_t>(p.children_pids.size());
        os.write(reinterpret_cast<const char*>(&child_count), sizeof(child_count));
        for (auto c : p.children_pids)
            os.write(reinterpret_cast<const char*>(&c), sizeof(c));

        // 内存块
        uint32_t mem_count = static_cast<uint32_t>(p.mem_block_addrs.size());
        os.write(reinterpret_cast<const char*>(&mem_count), sizeof(mem_count));
        for (size_t i = 0; i < p.mem_block_addrs.size(); ++i) {
            os.write(reinterpret_cast<const char*>(&p.mem_block_addrs[i]), sizeof(uint32_t));
            os.write(reinterpret_cast<const char*>(&p.mem_block_sizes[i]), sizeof(uint32_t));
        }
    }
}

void ProcessManager::deserialize(std::istream& is) {
    pcbs_.clear();
    uint32_t count = 0;
    is.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (is.fail()) return;
    is.read(reinterpret_cast<char*>(&next_pid_), sizeof(next_pid_));

    for (uint32_t i = 0; i < count; ++i) {
        PCB p;
        is.read(reinterpret_cast<char*>(&p.pid), sizeof(p.pid));

        uint16_t name_len = 0;
        is.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        p.name.resize(name_len);
        is.read(&p.name[0], name_len);

        uint8_t state_byte = 0;
        is.read(reinterpret_cast<char*>(&state_byte), sizeof(state_byte));
        p.state = static_cast<ProcessState>(state_byte);

        is.read(reinterpret_cast<char*>(&p.priority), sizeof(p.priority));
        is.read(reinterpret_cast<char*>(&p.parent_pid), sizeof(p.parent_pid));

        uint16_t owner_len = 0;
        is.read(reinterpret_cast<char*>(&owner_len), sizeof(owner_len));
        p.owner.resize(owner_len);
        is.read(&p.owner[0], owner_len);

        is.read(reinterpret_cast<char*>(&p.creation_time), sizeof(p.creation_time));
        is.read(reinterpret_cast<char*>(&p.required_time), sizeof(p.required_time));
        is.read(reinterpret_cast<char*>(&p.total_cpu_ticks), sizeof(p.total_cpu_ticks));
        is.read(reinterpret_cast<char*>(&p.queue_level), sizeof(p.queue_level));

        uint8_t suspended_byte = 0;
        is.read(reinterpret_cast<char*>(&suspended_byte), sizeof(suspended_byte));
        p.suspended = (suspended_byte != 0);

        uint8_t swapped_byte = 0;
        is.read(reinterpret_cast<char*>(&swapped_byte), sizeof(swapped_byte));
        p.swapped_out = (swapped_byte != 0);
        is.read(reinterpret_cast<char*>(&p.swapped_size), sizeof(p.swapped_size));

        uint32_t child_count = 0;
        is.read(reinterpret_cast<char*>(&child_count), sizeof(child_count));
        p.children_pids.resize(child_count);
        for (uint32_t j = 0; j < child_count; ++j)
            is.read(reinterpret_cast<char*>(&p.children_pids[j]), sizeof(uint32_t));

        uint32_t mem_count = 0;
        is.read(reinterpret_cast<char*>(&mem_count), sizeof(mem_count));
        p.mem_block_addrs.resize(mem_count);
        p.mem_block_sizes.resize(mem_count);
        for (uint32_t j = 0; j < mem_count; ++j) {
            is.read(reinterpret_cast<char*>(&p.mem_block_addrs[j]), sizeof(uint32_t));
            is.read(reinterpret_cast<char*>(&p.mem_block_sizes[j]), sizeof(uint32_t));
        }
        pcbs_.push_back(p);
    }
}
