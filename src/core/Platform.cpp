#include "core/Platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#include <chrono>
#include <sys/stat.h>
#endif

#include <functional>
#include <sstream>

namespace Platform {

void sleep_ms(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

uint64_t file_last_write_time_ms(const std::string& path) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    FILETIME ftWrite;
    uint64_t result = 0;
    if (GetFileTime(hFile, nullptr, nullptr, &ftWrite)) {
        ULARGE_INTEGER uli;
        uli.LowPart  = ftWrite.dwLowDateTime;
        uli.HighPart = ftWrite.dwHighDateTime;
        // FILETIME is 100-nanosecond intervals since 1601-01-01
        // Convert to milliseconds since epoch
        result = (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
    }
    CloseHandle(hFile);
    return result;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_mtime) * 1000;
#endif
}

bool file_exists(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

uint64_t current_time_ms() {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
#else
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
#endif
}

std::string hash_password(const std::string& password) {
    // 简单哈希：std::hash 组合
    // 课设演示用途，实际不会用于生产环境
    std::size_t h = std::hash<std::string>{}(password);
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

} // namespace Platform
