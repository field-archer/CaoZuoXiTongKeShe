#pragma once

#include "core/Types.h"
#include "thread/Message.h"

/// 线程安全消息队列 —— 生产者-消费者模型
/// 前台线程 push，后台线程 pop
class SharedQueue {
public:
    SharedQueue() = default;
    ~SharedQueue() = default;

    // 禁止拷贝
    SharedQueue(const SharedQueue&) = delete;
    SharedQueue& operator=(const SharedQueue&) = delete;

    /// 向队列投递消息（前台线程调用）
    void push(Message msg);

    /// 阻塞取出消息（后台线程调用，队列空时阻塞等待）
    Message pop();

    /// 带超时的取出（用于多实例轮询，超时返回 nullopt）
    std::optional<Message> try_pop_for(uint32_t timeout_ms);

    /// 通知队列关闭，唤醒所有等待线程
    void shutdown();

    /// 队列是否已关闭
    bool is_shutdown() const;

    /// 当前队列长度
    size_t size() const;

private:
    std::queue<Message>     queue_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    bool                    shutdown_ = false;
};
