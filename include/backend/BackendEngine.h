#pragma once

#include "core/Types.h"
#include "thread/SharedQueue.h"
#include "thread/FileLock.h"
#include "user/UserManager.h"
#include "backend/UserState.h"
#include "persist/StateFile.h"
#include <unordered_map>

/// 后台引擎
class BackendEngine {
public:
    BackendEngine(SharedQueue& queue, const std::string& state_path, bool standalone = false);
    ~BackendEngine();

    BackendEngine(const BackendEngine&) = delete;
    BackendEngine& operator=(const BackendEngine&) = delete;

    void run();
    std::string process(const Message& msg, const std::string& username);

private:
    SharedQueue&  queue_;
    std::string   state_path_;

    // 共享
    UserManager user_mgr_;
    // 每用户独立状态
    std::unordered_map<std::string, UserState> users_;

    FileLock    file_lock_;
    bool        is_writer_ = false;
    uint64_t    last_file_mtime_ = 0;

    std::atomic<bool> write_busy_{false};
    std::mutex         process_mtx_;

    UserState& get_or_create_user(const std::string& username);

    std::string dispatch(const Message& msg, const std::string& username);
    std::string overview(const std::string& username);

    void try_acquire_write_lock();
    void check_and_reload_if_changed();
    void load_state();
    void save_state();

    static std::string arg(const std::vector<std::string>& args, size_t idx,
                           const std::string& def = "") {
        return idx < args.size() ? args[idx] : def;
    }
};
