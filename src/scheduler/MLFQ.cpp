#include "scheduler/MLFQ.h"
#include "core/Config.h"
#include <sstream>
#include <algorithm>
#include "process/ProcessManager.h"
#include "scheduler/Scheduler.h"

MLFQ::MLFQ() = default;

void MLFQ::enqueue(uint32_t pid, int priority) {
    int level = MLFQ_LEVEL_FOR_PRIORITY(priority);
    enqueue_to_level(pid, level);
}

void MLFQ::enqueue_to_level(uint32_t pid, int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    queues_[level].push_back(pid);
}

uint32_t MLFQ::dequeue() {
    for (int i = 0; i < 3; ++i) {
        if (!queues_[i].empty()) {
            uint32_t pid = queues_[i].front();
            queues_[i].pop_front();
            return pid;
        }
    }
    return 0;
}

uint32_t MLFQ::dequeue_from(int level) {
    if (level < 0 || level > 2) return 0;
    if (queues_[level].empty()) return 0;
    uint32_t pid = queues_[level].front();
    queues_[level].pop_front();
    return pid;
}

bool MLFQ::remove(uint32_t pid) {
    for (int i = 0; i < 3; ++i) {
        auto& q = queues_[i];
        auto it = std::find(q.begin(), q.end(), pid);
        if (it != q.end()) {
            q.erase(it);
            return true;
        }
    }
    return false;
}

void MLFQ::demote(uint32_t pid) {
    int level = find_level(pid);
    if (level < 0) return; // 不在队列中

    if (level < 2) {
        // 从当前队列移除，加入下一级
        remove(pid);
        enqueue_to_level(pid, level + 1);
    }
    // Q2 保持不动
}

int MLFQ::find_level(uint32_t pid) const {
    for (int i = 0; i < 3; ++i) {
        const auto& q = queues_[i];
        if (std::find(q.begin(), q.end(), pid) != q.end()) {
            return i;
        }
    }
    return -1;
}

std::string MLFQ::snapshot(const ProcessManager* pm, const std::string& owner) const {
    std::ostringstream oss;
    for (int i = 0; i < 3; ++i) {
        oss << "Q" << i << "): [";
        bool first = true;
        for (auto pid : queues_[i]) {
            if (pm) {
                const PCB* pcb = pm->get_pcb(pid);
                if (!pcb) continue;
                if (!owner.empty() && pcb->owner != owner && pid != 1) continue;
                if (!first) oss << ", ";
                oss << pcb->name << "(" << pid << "," << pcb->remaining_time() << ")";
            } else {
                if (!first) oss << ", ";
                oss << "PID " << pid;
            }
            first = false;
        }
        oss << "]";
        if (i < 2) oss << std::endl;
    }
    return oss.str();
}

const std::deque<uint32_t>& MLFQ::queue(int level) const {
    return queues_[level];
}

void MLFQ::clear() {
    for (auto& q : queues_) {
        q.clear();
    }
}

void MLFQ::serialize(std::ostream& os) const {
    for (int i = 0; i < 3; ++i) {
        uint32_t len = static_cast<uint32_t>(queues_[i].size());
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        for (auto pid : queues_[i]) {
            os.write(reinterpret_cast<const char*>(&pid), sizeof(pid));
        }
    }
}

void MLFQ::deserialize(std::istream& is) {
    for (int i = 0; i < 3; ++i) {
        queues_[i].clear();
        uint32_t len = 0;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        for (uint32_t j = 0; j < len; ++j) {
            uint32_t pid = 0;
            is.read(reinterpret_cast<char*>(&pid), sizeof(pid));
            queues_[i].push_back(pid);
        }
    }
}
