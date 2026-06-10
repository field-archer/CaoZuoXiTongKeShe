#pragma once

#include "core/Types.h"

class ProcessManager;

/// 多级反馈队列 (Multi-Level Feedback Queue)
/// Q0: 优先级 0-3,  时间片 2
/// Q1: 优先级 4-7,  时间片 4
/// Q2: 优先级 8-15, 时间片 8
/// 调度策略：严格优先级，高优先级非空时绝不自低优先级取进程
class MLFQ {
public:
    MLFQ();

    /// 将进程加入对应优先级队列
    void enqueue(uint32_t pid, int priority);

    /// 将进程加入指定层级队列（用于阻塞唤醒等场景）
    void enqueue_to_level(uint32_t pid, int level);

    /// 从最高优先级非空队列取出一个进程 PID，返回 0 表示全空
    uint32_t dequeue();

    /// 从指定层级队列取出队首进程 PID
    uint32_t dequeue_from(int level);

    /// 从队列中移除指定进程
    bool remove(uint32_t pid);

    /// 进程降级（时间片耗尽）：Q0→Q1, Q1→Q2, Q2 不动
    void demote(uint32_t pid);

    /// 查询进程所在队列层级，返回 -1 表示不在任何队列
    int find_level(uint32_t pid) const;

    /// 队列快照，owner 非空时只显示该用户的进程
    std::string snapshot(const ProcessManager* pm = nullptr,
                         const std::string& owner = "") const;

    /// 获取各队列引用（供 overview 使用）
    const std::deque<uint32_t>& queue(int level) const;

    /// 持久化
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);

    /// 清空所有队列
    void clear();

private:
    std::array<std::deque<uint32_t>, 3> queues_;  // Q0, Q1, Q2
};
