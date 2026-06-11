#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <mutex>
class Int64UUIDGenerator {
public:
    // 自定义纪元起点 (2020-01-01 00:00:00 UTC)
    static constexpr std::int64_t EPOCH = 1577836800000LL; // milliseconds

    // 各段位宽
    static constexpr int TIMESTAMP_BITS = 41;
    static constexpr int MACHINE_ID_BITS = 10;
    static constexpr int SEQUENCE_BITS = 12;

    // 最大值 & 移位量
    static constexpr std::int64_t MAX_MACHINE_ID   = (1LL << MACHINE_ID_BITS) - 1;   // 1023
    static constexpr std::int64_t MAX_SEQUENCE     = (1LL << SEQUENCE_BITS) - 1;     // 4095
    static constexpr int MACHINE_SHIFT = SEQUENCE_BITS;
    static constexpr int TIMESTAMP_SHIFT = SEQUENCE_BITS + MACHINE_ID_BITS;

    // 构造函数：传入机器ID (0 ~ 1023)
    explicit Int64UUIDGenerator(std::int64_t machineId)
        : machine_id_(machineId), sequence_(0), last_timestamp_(-1) {
        if (machineId < 0 || machineId > MAX_MACHINE_ID) {
            throw std::invalid_argument("Machine ID out of range");
        }
    }

    // 生成下一个唯一ID
    std::int64_t nextId() {
        std::int64_t now = getCurrentTimestamp();
        // 处理时钟回拨（简单策略：等待直到追上）
        if (now < last_timestamp_) {
            // 生产环境建议采用更复杂的策略（如记录回拨量或抛出异常）
            std::this_thread::sleep_for(std::chrono::milliseconds(last_timestamp_ - now));
            now = getCurrentTimestamp();
            if (now < last_timestamp_) {
                throw std::runtime_error("Clock moved backwards too much");
            }
        }

        // 同一毫秒内序列号自增
        if (now == last_timestamp_) {
            sequence_ = (sequence_ + 1) & MAX_SEQUENCE;
            if (sequence_ == 0) { // 当前毫秒内序列号用完
                // 等待下一毫秒
                while (now <= last_timestamp_) {
                    now = getCurrentTimestamp();
                }
            }
        } else {
            sequence_ = 0; // 新毫秒重置序列号
        }

        last_timestamp_ = now;

        std::int64_t id = ((now - EPOCH) << TIMESTAMP_SHIFT)
                        | (machine_id_ << MACHINE_SHIFT)
                        | sequence_;
        return id;
    }

private:
    const std::int64_t machine_id_;
    std::int64_t sequence_;
    std::int64_t last_timestamp_;
    std::mutex mutex_;  // 保证线程安全

    // 获取当前毫秒时间戳
    std::int64_t getCurrentTimestamp() const {
        using namespace std::chrono;
        auto now = system_clock::now().time_since_epoch();
        return duration_cast<milliseconds>(now).count();
    }
};