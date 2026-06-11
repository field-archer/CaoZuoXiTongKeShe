#include "scheduler/MLFQ.h"
#include "core/Config.h"
#include <sstream>
#include <algorithm>
#include "process/ProcessManager.h"

MLFQ::MLFQ() = default;

void MLFQ::enqueue_to_level(uint32_t pid, int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    queues_[level].push_back(pid);
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
        auto it = std::find(queues_[i].begin(), queues_[i].end(), pid);
        if (it != queues_[i].end()) { queues_[i].erase(it); return true; }
    }
    return false;
}

std::string MLFQ::snapshot(const ProcessManager* pm) const {
    std::ostringstream oss;
    for (int i = 0; i < 3; ++i) {
        oss << "Q" << i << "): [";
        bool first = true;
        for (auto pid : queues_[i]) {
            if (!first) oss << ", ";
            if (pm) {
                const PCB* pcb = pm->get_pcb(pid);
                oss << (pcb ? pcb->name : "?") << "(" << pid
                    << "," << (pcb ? pcb->remaining_time() : 0) << ")";
            } else {
                oss << "PID " << pid;
            }
            first = false;
        }
        oss << "]";
        if (i < 2) oss << std::endl;
    }
    return oss.str();
}

const std::deque<uint32_t>& MLFQ::queue(int level) const { return queues_[level]; }

void MLFQ::clear() { for (auto& q : queues_) q.clear(); }

void MLFQ::serialize(std::ostream& os) const {
    for (int i = 0; i < 3; ++i) {
        uint32_t len = static_cast<uint32_t>(queues_[i].size());
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        for (auto pid : queues_[i])
            os.write(reinterpret_cast<const char*>(&pid), sizeof(pid));
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
