#pragma once

#include "core/Types.h"
#include "backend/UserState.h"
#include <string>
#include <unordered_map>

class UserManager;

class StateFile {
public:
    explicit StateFile(const std::string& path);
    ~StateFile() = default;

    bool save(UserManager& um, const std::unordered_map<std::string, UserState>& users);
    bool load(UserManager& um, std::unordered_map<std::string, UserState>& users);

    bool exists() const;
    const std::string& path() const { return path_; }

private:
    std::string path_;

    static constexpr const char MAGIC[4] = {'O', 'S', 'C', 'S'};
    static constexpr uint32_t VERSION = 4;
};
