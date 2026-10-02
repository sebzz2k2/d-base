#include "wal.h"
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

wal::wal(const WALConfig &config)
    : processing_(false), config_(config)
{
}

void wal::add_wal_entry(int i) // TODO: change to type WAL
{
    if (wal::q_.size() == wal::config_.queue_capacity)
    {
        wal::bulk_flush_();
    }

    if (wal::mtx_.try_lock())
    {
        wal::q_.push(i); // TODO: check if emplace is needed
        wal::mtx_.unlock();
    }
}

void wal::loop()
{
    while (true)
    {
        wal::bulk_flush_();
        std::this_thread::sleep_for(std::chrono::milliseconds(wal::config_.flush_interval_ms));
    }
}

WALWriteResult wal::bulk_flush_()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (q_.empty())
        return WALWriteResult::Durable;

    // TODO: replace this path with the configured WAL file path.
    const int fd = open("/data/", O_WRONLY | O_CREAT | O_APPEND,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        ++write_failed_count_;
        if (write_failed_count_ >= config_.max_write_fails)
            throw std::runtime_error("Unable to write WAL after multiple failures: " + std::string(std::strerror(errno)));
        return WALWriteResult::WriteFailed;
    }

    std::string str = "";
    while (wal::q_.size() != 0)
    {
        int val = wal::q_.front();
        // TODO: build WAL entry and append to string
    }

    if (write(fd, "", str.length()) == -1)
    {
        ++write_failed_count_;
        close(fd);
        return WALWriteResult::WriteFailed;
    }
    write_failed_count_ = 0;

    while (fsync(fd) == -1)
    {
        ++fsync_failed_count_;
        if (fsync_failed_count_ >= config_.max_fsync_fails)
        {
            close(fd);
            throw std::runtime_error("Unable to sync WAL after multiple failures: " + std::string(std::strerror(errno)));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    fsync_failed_count_ = 0;
    close(fd);

    return WALWriteResult::Durable;
}