#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <future>
#include <cstdint>
#include <cstddef>
#include <string>
#include "internal/cmd/commands.h"

enum class WALWriteResult
{
    Durable,
    WriteFailed,
    SyncFailed
};

enum class WALRecordType : std::uint8_t
{
    FULL = 1,
    FIRST = 2,
    MIDDLE = 3,
    LAST = 4
};

struct WALConfig
{
    unsigned flush_interval_ms = 50;
    unsigned int queue_capacity = 1000;
    const unsigned int max_fsync_fails = 5;
    const unsigned int max_write_fails = 5;
};

struct WALRecord
{
    std::uint64_t lsn = 0;
    std::uint32_t txn_id;
    WALRecordType type = WALRecordType::FULL;
    VaulticCmds command;
    std::string key;
    std::string value;
};

struct WALRequest
{
    WALRecord record;
    std::promise<uint8_t> completion;
};
class wal
{
public:
    wal(const WALConfig &config);

    void add_wal_entry(WALRequest req);
    void loop();

private:
    bool processing_;
    int write_failed_count_ = 0;
    int fsync_failed_count_ = 0;
    std::uint64_t next_lsn_ = 1;

    std::mutex mtx_;
    // TODO: might want to implement queue
    std::queue<WALRequest> q_;
    WALConfig config_;

    WALWriteResult bulk_flush_();
    std::string marshal_(const WALRecord *record, std::size_t &block_offset);
    void unmarshall_();
};
