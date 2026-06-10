#include "thread/SharedQueue.h"

void SharedQueue::push(Message msg) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (shutdown_) return;
        queue_.push(std::move(msg));
    }
    cv_.notify_one();
}

Message SharedQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] {
        return !queue_.empty() || shutdown_;
    });

    if (shutdown_ && queue_.empty()) {
        // 返回一个退出信号消息
        Message exit_msg;
        exit_msg.type = MessageType::EXIT;
        return exit_msg;
    }

    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
}

std::optional<Message> SharedQueue::try_pop_for(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx_);
    bool has_msg = cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
        return !queue_.empty() || shutdown_;
    });

    if (!has_msg) {
        return std::nullopt;
    }

    if (shutdown_ && queue_.empty()) {
        return std::nullopt;
    }

    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
}

void SharedQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        shutdown_ = true;
    }
    cv_.notify_all();
}

bool SharedQueue::is_shutdown() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return shutdown_;
}

size_t SharedQueue::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.size();
}
