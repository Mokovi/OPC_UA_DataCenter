#include "redis_client.hpp"
#include <hiredis/hiredis.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <optional>

namespace data_processor {

HiredisAsyncClient::HiredisAsyncClient(const RedisConfig& config)
    : config_(config)
    , redis_context_(nullptr)
    , running_(false)
    , total_operations_(0)
    , successful_operations_(0)
    , failed_operations_(0) {
}

HiredisAsyncClient::~HiredisAsyncClient() {
    stop();
}

bool HiredisAsyncClient::start() {
    if (running_) {
        std::cout << "Redis client is already running" << std::endl;
        return true;
    }

    std::cout << "Starting Redis client, connecting to " << config_.host
              << ":" << config_.port << std::endl;

    // 创建连接
    if (!createConnection()) {
        std::cerr << "Failed to create Redis connection" << std::endl;
        return false;
    }

    // 启动工作线程
    running_ = true;
    worker_thread_ = std::thread(&HiredisAsyncClient::workerThread, this);

    std::cout << "Redis client started successfully" << std::endl;
    return true;
}

void HiredisAsyncClient::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 通知工作线程停止
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_cv_.notify_all();
    }

    // 等待工作线程结束
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // 关闭连接
    closeConnection();

    std::cout << "Redis client stopped" << std::endl;
}

std::string HiredisAsyncClient::getStatus() const {
    if (!running_) {
        return "Stopped";
    }

    if (!redis_context_) {
        return "Disconnected";
    }

    return "Connected";
}

void HiredisAsyncClient::storeDataPointAsync(const DataPoint& data_point,
                                           std::function<void(RedisResult)> callback) {
    if (!running_) {
        if (callback) {
            callback(RedisResult::ConnectionError);
        }
        return;
    }

    AsyncTask task(AsyncTask::Type::StoreSingle, data_point, callback);

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void HiredisAsyncClient::storeDataPointsAsync(const std::vector<DataPoint>& data_points,
                                            std::function<void(RedisResult, size_t)> callback) {
    if (!running_) {
        if (callback) {
            callback(RedisResult::ConnectionError, 0);
        }
        return;
    }

    AsyncTask task(AsyncTask::Type::StoreBatch, data_points, callback);

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

std::optional<DataPoint> HiredisAsyncClient::getDataPoint(const std::string& source_id,
                                                        const std::string& node_id) {
    if (!redis_context_) {
        return std::nullopt;
    }

    redisContext* context = static_cast<redisContext*>(redis_context_);
    std::string key = generateDataPointKey(source_id, node_id);

    // 获取所有字段
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context, "HGETALL %s", key.c_str()));

    if (!reply) {
        return std::nullopt;
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply_guard(reply, freeReplyObject);

    if (reply->type != REDIS_REPLY_ARRAY || reply->elements < 6) { // 3个字段 * 2 (键值对)
        return std::nullopt;
    }

    DataPoint data_point;
    data_point.source_id = source_id;
    data_point.node_id = node_id;

    // 解析字段值
    for (size_t i = 0; i < reply->elements; i += 2) {
        if (i + 1 >= reply->elements) break;

        std::string field_name = reply->element[i]->str;
        std::string field_value = reply->element[i + 1]->str;

        if (field_name == "value") {
            data_point.value = field_value;
        } else if (field_name == "updated_at") {
            try {
                data_point.timestamp = std::stoll(field_value);
            } catch (const std::exception&) {
                data_point.timestamp = 0;
            }
        } else if (field_name == "quality") {
            try {
                data_point.quality = std::stoi(field_value);
            } catch (const std::exception&) {
                data_point.quality = 0;
            }
        }
    }

    return data_point;
}

void HiredisAsyncClient::cleanupExpiredData(int max_age_seconds) {
    if (!running_) {
        return;
    }

    AsyncTask task(AsyncTask::Type::Cleanup, max_age_seconds);

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

std::tuple<size_t, size_t, size_t> HiredisAsyncClient::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {total_operations_.load(), successful_operations_.load(), failed_operations_.load()};
}

void HiredisAsyncClient::workerThread() {
    while (running_) {
        AsyncTask task(AsyncTask::Type::StoreSingle, DataPoint{});

        // 等待任务
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !running_ || !task_queue_.empty();
            });

            if (!running_ && task_queue_.empty()) {
                break;
            }

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            } else {
                continue;
            }
        }

        // 处理任务
        total_operations_++;

        switch (task.type) {
            case AsyncTask::Type::StoreSingle: {
                RedisResult result = storeDataPointInternal(task.data_point);
                if (result == RedisResult::Success) {
                    successful_operations_++;
                } else {
                    failed_operations_++;
                }

                if (task.single_callback) {
                    task.single_callback(result);
                }
                break;
            }

            case AsyncTask::Type::StoreBatch: {
                size_t success_count = 0;
                RedisResult overall_result = RedisResult::Success;

                for (const auto& data_point : task.data_points) {
                    RedisResult result = storeDataPointInternal(data_point);
                    if (result == RedisResult::Success) {
                        success_count++;
                    } else {
                        overall_result = result;
                    }
                }

                total_operations_ += task.data_points.size() - 1; // 已经算过一个了
                successful_operations_ += success_count;
                failed_operations_ += (task.data_points.size() - success_count);

                if (task.batch_callback) {
                    task.batch_callback(overall_result, success_count);
                }
                break;
            }

            case AsyncTask::Type::Cleanup: {
                // 清理过期数据的实现
                // 这里简化实现，实际应该扫描所有键并删除过期的
                std::cout << "Cleanup task received, max_age: " << task.max_age_seconds << " seconds" << std::endl;

                if (task.single_callback) {
                    task.single_callback(RedisResult::Success);
                }
                break;
            }
        }
    }
}

bool HiredisAsyncClient::createConnection() {
    struct timeval timeout = {
        config_.connection_timeout_ms / 1000,
        (config_.connection_timeout_ms % 1000) * 1000
    };

    redisContext* context = redisConnectWithTimeout(config_.host.c_str(), config_.port, timeout);

    if (!context) {
        std::cerr << "Failed to allocate Redis context" << std::endl;
        return false;
    }

    if (context->err) {
        std::cerr << "Redis connection error: " << context->errstr << std::endl;
        redisFree(context);
        return false;
    }

    redis_context_ = context;

    // 设置数据库
    if (config_.db_index != 0) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "SELECT %d", config_.db_index));

        if (!reply || reply->type != REDIS_REPLY_STATUS ||
            std::string(reply->str) != "OK") {
            std::cerr << "Failed to select Redis database " << config_.db_index << std::endl;
            if (reply) freeReplyObject(reply);
            closeConnection();
            return false;
        }
        freeReplyObject(reply);
    }

    // 设置密码
    if (!config_.password.empty()) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "AUTH %s", config_.password.c_str()));

        if (!reply || reply->type != REDIS_REPLY_STATUS ||
            std::string(reply->str) != "OK") {
            std::cerr << "Redis authentication failed" << std::endl;
            if (reply) freeReplyObject(reply);
            closeConnection();
            return false;
        }
        freeReplyObject(reply);
    }

    return true;
}

void HiredisAsyncClient::closeConnection() {
    if (redis_context_) {
        redisFree(static_cast<redisContext*>(redis_context_));
        redis_context_ = nullptr;
    }
}

RedisResult HiredisAsyncClient::storeDataPointInternal(const DataPoint& data_point) {
    if (!redis_context_) {
        return RedisResult::ConnectionError;
    }

    redisContext* context = static_cast<redisContext*>(redis_context_);
    std::string key = generateDataPointKey(data_point.source_id, data_point.node_id);

    // 使用 HMSET 设置三个字段：value, updated_at, quality
    int64_t updated_at = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context, "HMSET %s value %s updated_at %lld quality %d",
                    key.c_str(),
                    data_point.value.c_str(),
                    updated_at,
                    data_point.quality));

    if (!reply) {
        std::cerr << "Redis command failed: connection lost" << std::endl;
        return RedisResult::ConnectionError;
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply_guard(reply, freeReplyObject);

    if (reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Redis command error: " << reply->str << std::endl;
        return RedisResult::UnknownError;
    }

    // 设置过期时间 (可选，防止无限增长)
    // 这里设置为 7 天过期
    redisReply* expire_reply = static_cast<redisReply*>(
        redisCommand(context, "EXPIRE %s %d", key.c_str(), 7 * 24 * 3600));

    if (expire_reply) {
        freeReplyObject(expire_reply);
    }

    return RedisResult::Success;
}

std::string HiredisAsyncClient::generateDataPointKey(const std::string& /*source_id*/,
                                                   const std::string& node_id) {
    // 生成 DataPoint:{node_id} 格式的键
    std::ostringstream oss;
    oss << "DataPoint:";

    // 对 node_id 进行编码，保持原有逻辑
    for (char c : node_id) {
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            oss << c;
        } else {
            oss << '_';
        }
    }

    return oss.str();
}

} // namespace data_processor
