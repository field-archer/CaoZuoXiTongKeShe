#include "thread/FileLock.h"

FileLock::FileLock(const std::string& path)
    : path_(path + ".lock") // 使用独立的 .lock 文件
{
}

FileLock::~FileLock() {
    release();
}

bool FileLock::acquire_exclusive() {
#ifdef _WIN32
    hFile_ = CreateFileA(
        path_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,          // 独占访问
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr
    );
    if (hFile_ == INVALID_HANDLE_VALUE) {
        has_lock_ = false;
        return false;
    }

    OVERLAPPED overlapped = {};
    if (!LockFileEx(hFile_, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0, MAXDWORD, MAXDWORD, &overlapped)) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
        has_lock_ = false;
        return false;
    }

    has_lock_ = true;
    return true;
#else
    fd_ = open(path_.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd_ < 0) {
        has_lock_ = false;
        return false;
    }
    if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        close(fd_);
        fd_ = -1;
        has_lock_ = false;
        return false;
    }
    has_lock_ = true;
    return true;
#endif
}

void FileLock::release() {
    if (!has_lock_) return;

#ifdef _WIN32
    UnlockFileEx(hFile_, 0, MAXDWORD, MAXDWORD, nullptr);
    CloseHandle(hFile_);
    hFile_ = INVALID_HANDLE_VALUE;
#else
    flock(fd_, LOCK_UN);
    close(fd_);
    fd_ = -1;
#endif
    has_lock_ = false;
}
