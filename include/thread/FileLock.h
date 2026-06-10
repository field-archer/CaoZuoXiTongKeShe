#pragma once

#include <string>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/// 跨平台文件锁（Windows: LockFileEx, Linux: flock）
class FileLock {
public:
    explicit FileLock(const std::string& path);
    ~FileLock();

    // 禁止拷贝
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    /// 尝试获取独占锁，返回是否成功
    bool acquire_exclusive();

    /// 释放锁
    void release();

    /// 当前是否持有锁
    bool has_lock() const { return has_lock_; }

private:
    std::string path_;
#ifdef _WIN32
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
    bool has_lock_ = false;
};
