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
#include "constants.h"
#include <utility>
#include <exception>

wal::wal(const WALConfig &config)
    : processing_(false), config_(config)
{
}

void wal::add_wal_entry(WALRequest req)
{
    if (wal::q_.size() == wal::config_.queue_capacity)
    {
        wal::bulk_flush_();
    }

    if (wal::mtx_.try_lock())
    {
        wal::q_.push(req); // TODO: check if emplace is needed
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

    const int fd = open(constants::wal_file.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        ++write_failed_count_;
        if (write_failed_count_ >= config_.max_write_fails)
            throw std::runtime_error("Unable to write WAL after multiple failures: " + std::string(std::strerror(errno)));
        return WALWriteResult::WriteFailed;
    }

    std::queue<WALRequest> write_success_q_;
    while (wal::q_.size() != 0)
    {
        WALRequest val = std::move(wal::q_.front());
        std::string str = wal::marshal_(&val.record);
        if (write(fd, "", str.length()) == -1)
        {
            val.completion.set_exception(std::make_exception_ptr(
                std::runtime_error("Failed to write WAL: " + std::string(std::strerror(errno)))));
        }
        write_success_q_.push(val);
        wal::q_.pop();
    }

    bool success = true;
    while (fsync(fd) == -1)
    {
        ++fsync_failed_count_;
        if (fsync_failed_count_ >= config_.max_fsync_fails)
        {
            close(fd);
            success = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    while (write_success_q_.size() != 0)
    {
        WALRequest val = std::move(write_success_q_.front());
        if (success == true)
        {
            val.completion.set_value(1);
        }
        else
        {
            val.completion.set_exception(std::make_exception_ptr(
                std::runtime_error("Failed to sync WAL after multiple attempts")));
        }
    }

    fsync_failed_count_ = 0;
    close(fd);

    return WALWriteResult::Durable;
}

std::string wal::marshal_(WALRecord *record)
{
    
}

void wal::unmarshall_()
{
}
