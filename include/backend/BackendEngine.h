#pragma once

#include "core/Types.h"
#include "thread/Message.h"
#include "user/UserManager.h"
#include "backend/UserState.h"
#include "persist/StateFile.h"
#include <unordered_map>

class BackendEngine {
public:
    BackendEngine(const std::string& state_path);
    ~BackendEngine();

    BackendEngine(const BackendEngine&) = delete;
    BackendEngine& operator=(const BackendEngine&) = delete;

    std::string process(const Message& msg, const std::string& username);

private:
    std::string   state_path_;
    UserManager   user_mgr_;
    std::unordered_map<std::string, UserState> users_;

    std::shared_mutex  process_mtx_;

    UserState& get_or_create_user(const std::string& username);

    std::string dispatch(const Message& msg, const std::string& username);

    void load_state();
    void save_state();

    static std::string arg(const std::vector<std::string>& args, size_t idx,
                           const std::string& def = "") {
        return idx < args.size() ? args[idx] : def;
    }
};
