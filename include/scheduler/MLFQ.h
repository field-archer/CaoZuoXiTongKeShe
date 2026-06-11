#pragma once

#include "core/Types.h"

class ProcessManager;

/// 多级反馈队列 Q0(2s)/Q1(4s)/Q2(8s)
class MLFQ {
public:
    MLFQ();

    void enqueue_to_level(uint32_t pid, int level);
    uint32_t dequeue_from(int level);
    bool remove(uint32_t pid);

    std::string snapshot(const ProcessManager* pm = nullptr) const;
    const std::deque<uint32_t>& queue(int level) const;

    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);
    void clear();

private:
    std::array<std::deque<uint32_t>, 3> queues_;
};
