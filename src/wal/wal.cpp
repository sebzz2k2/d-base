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
#include <algorithm>

namespace
{
    constexpr std::size_t kBlockSize = 32 * 1024;
    constexpr std::size_t kHeaderSize = sizeof(std::uint32_t) +
                                        sizeof(std::uint16_t) +
                                        sizeof(std::uint8_t) +
                                        sizeof(std::uint32_t);

    void append_u16(std::string &out, std::uint16_t value)
    {
        out.push_back(static_cast<char>(value));
        out.push_back(static_cast<char>(value >> 8));
    }

    void append_u32(std::string &out, std::uint32_t value)
    {
        for (unsigned shift = 0; shift < 32; shift += 8)
            out.push_back(static_cast<char>(value >> shift));
    }

    std::uint32_t crc32(const std::string &data)
    {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (unsigned char byte : data)
        {
            crc ^= byte;
            for (int i = 0; i < 8; ++i)
                crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
        }
        return ~crc;
    }
}

wal::wal(const WALConfig &config)
    : processing_(false), config_(config)
{
}

void wal::add_wal_entry(WALRequest req)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (q_.size() >= config_.queue_capacity)
    {
        req.completion.set_exception(std::make_exception_ptr(
            std::runtime_error("WAL queue is full")));
        return;
    }

    q_.push(std::move(req));
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
    std::string batch;
    std::size_t block_offset = 0;
    while (wal::q_.size() != 0)
    {
        WALRequest val = std::move(wal::q_.front());
        batch += wal::marshal_(&val.record, block_offset);
        write_success_q_.push(std::move(val));
        wal::q_.pop();
    }

    std::size_t offset = 0;
    while (offset < batch.size())
    {
        const ssize_t written = write(fd, batch.data() + offset, batch.size() - offset);
        if (written < 0)
        {
            const auto error = std::make_exception_ptr(
                std::runtime_error("Failed to write WAL: " +
                                   std::string(std::strerror(errno))));
            while (!write_success_q_.empty())
            {
                write_success_q_.front().completion.set_exception(error);
                write_success_q_.pop();
            }
            close(fd);
            return WALWriteResult::WriteFailed;
        }
        offset += static_cast<std::size_t>(written);
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
        write_success_q_.pop();
    }

    fsync_failed_count_ = 0;
    close(fd);

    return WALWriteResult::Durable;
}

std::string wal::marshal_(const WALRecord *record, std::size_t &block_offset)
{
    // Logical record payload: [command: uint8][key length: uint32][key][value]
    std::string payload;
    payload.push_back(static_cast<char>(record->command));
    append_u32(payload, static_cast<std::uint32_t>(record->key.size()));
    payload += record->key;
    payload += record->value;

    std::string result;
    std::size_t payload_offset = 0;
    const bool fragmented = payload.size() + kHeaderSize > kBlockSize;

    while (payload_offset < payload.size() || payload.empty())
    {
        const std::size_t remaining = kBlockSize - block_offset;

        // A fragment header must not cross a block boundary.
        if (remaining <= kHeaderSize)
        {
            result.append(remaining, '\0');
            block_offset = 0;
        }

        const std::size_t available = kBlockSize - block_offset - kHeaderSize;
        const std::size_t fragment_size =
            std::min(available, payload.size() - payload_offset);
        const bool first = payload_offset == 0;
        const bool last = payload_offset + fragment_size == payload.size();

        WALRecordType type;
        if (!fragmented)
            type = WALRecordType::FULL;
        else if (first)
            type = WALRecordType::FIRST;
        else if (last)
            type = WALRecordType::LAST;
        else
            type = WALRecordType::MIDDLE;

        std::string body;
        append_u16(body, static_cast<std::uint16_t>(fragment_size));
        body.push_back(static_cast<char>(type));
        append_u32(body, record->txn_id);
        body.append(payload, payload_offset, fragment_size);

        append_u32(result, crc32(body));
        result += body;

        block_offset += kHeaderSize + fragment_size;
        if (block_offset == kBlockSize)
            block_offset = 0;

        payload_offset += fragment_size;
        if (payload.empty())
            break;
    }

    return result;
}

void wal::unmarshall_()
{
}
