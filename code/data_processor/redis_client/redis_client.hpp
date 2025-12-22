#pragma once

#include "../utilities/config.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <optional>

namespace data_processor {

// RedisConfig is defined in config.hpp

/**
 * @brief Redis 异步操作结果
 */
enum class RedisResult {
    Success = 0,      ///< 成功
    ConnectionError,  ///< 连接错误
    Timeout,          ///< 超时
    KeyNotFound,      ///< 键不存在
    InvalidData,      ///< 数据无效
    UnknownError      ///< 未知错误
};

/**
 * @brief 数据点信息
 */
struct DataPoint {
    std::string source_id;       ///< 数据源标识
    std::string node_id;         ///< 节点ID
    std::string value;           ///< 数据值
    int64_t timestamp;           ///< 时间戳
    int quality;                 ///< 数据质量
};

/**
 * @brief Redis 客户端接口
 */
class IRedisClient {
public:
    virtual ~IRedisClient() = default;

    /**
     * @brief 启动客户端
     * @return 启动是否成功
     */
    virtual bool start() = 0;

    /**
     * @brief 停止客户端
     */
    virtual void stop() = 0;

    /**
     * @brief 获取客户端状态
     * @return 状态描述字符串
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief 异步存储数据点到 Redis Hash
     * @param data_point 数据点
     * @param callback 完成回调函数
     */
    virtual void storeDataPointAsync(const DataPoint& data_point,
                                    std::function<void(RedisResult)> callback = nullptr) = 0;

    /**
     * @brief 批量异步存储数据点
     * @param data_points 数据点列表
     * @param callback 完成回调函数
     */
    virtual void storeDataPointsAsync(const std::vector<DataPoint>& data_points,
                                     std::function<void(RedisResult, size_t)> callback = nullptr) = 0;

    /**
     * @brief 同步获取数据点
     * @param source_id 数据源ID
     * @param node_id 节点ID
     * @return 数据点信息，如果不存在返回空
     */
    virtual std::optional<DataPoint> getDataPoint(const std::string& source_id,
                                                 const std::string& node_id) = 0;

    /**
     * @brief 清理过期数据
     * @param max_age_seconds 最大年龄（秒）
     */
    virtual void cleanupExpiredData(int max_age_seconds) = 0;

    /**
     * @brief 获取统计信息
     * @return 统计信息 (总操作数, 成功数, 失败数)
     */
    virtual std::tuple<size_t, size_t, size_t> getStats() const = 0;
};

/**
 * @brief 基于 hiredis 的高性能异步 Redis 客户端
 */
class HiredisAsyncClient : public IRedisClient {
public:
    /**
     * @brief 构造函数
     * @param config Redis 配置
     */
    explicit HiredisAsyncClient(const RedisConfig& config);

    /**
     * @brief 析构函数
     */
    ~HiredisAsyncClient() override;

    /**
     * @brief 启动客户端
     * @return 启动是否成功
     */
    bool start() override;

    /**
     * @brief 停止客户端
     */
    void stop() override;

    /**
     * @brief 获取客户端状态
     * @return 状态描述字符串
     */
    std::string getStatus() const override;

    /**
     * @brief 异步存储数据点到 Redis Hash
     * @param data_point 数据点
     * @param callback 完成回调函数
     */
    void storeDataPointAsync(const DataPoint& data_point,
                            std::function<void(RedisResult)> callback = nullptr) override;

    /**
     * @brief 批量异步存储数据点
     * @param data_points 数据点列表
     * @param callback 完成回调函数
     */
    void storeDataPointsAsync(const std::vector<DataPoint>& data_points,
                             std::function<void(RedisResult, size_t)> callback = nullptr) override;

    /**
     * @brief 同步获取数据点
     * @param source_id 数据源ID
     * @param node_id 节点ID
     * @return 数据点信息，如果不存在返回空
     */
    std::optional<DataPoint> getDataPoint(const std::string& source_id,
                                         const std::string& node_id) override;

    /**
     * @brief 清理过期数据
     * @param max_age_seconds 最大年龄（秒）
     */
    void cleanupExpiredData(int max_age_seconds) override;

    /**
     * @brief 获取统计信息
     * @return 统计信息 (总操作数, 成功数, 失败数)
     */
    std::tuple<size_t, size_t, size_t> getStats() const override;

private:
    /**
     * @brief 异步操作任务
     */
    struct AsyncTask {
        enum class Type { StoreSingle, StoreBatch, Cleanup };

        Type type;
        DataPoint data_point;
        std::vector<DataPoint> data_points;
        int max_age_seconds;
        std::function<void(RedisResult)> single_callback;
        std::function<void(RedisResult, size_t)> batch_callback;

        AsyncTask(Type t, const DataPoint& dp, std::function<void(RedisResult)> cb = nullptr)
            : type(t), data_point(dp), single_callback(cb) {}

        AsyncTask(Type t, const std::vector<DataPoint>& dps,
                 std::function<void(RedisResult, size_t)> cb = nullptr)
            : type(t), data_points(dps), batch_callback(cb) {}

        AsyncTask(Type t, int max_age, std::function<void(RedisResult)> cb = nullptr)
            : type(t), max_age_seconds(max_age), single_callback(cb) {}
    };

    /**
     * @brief 工作线程函数
     */
    void workerThread();

    /**
     * @brief 创建 Redis 连接
     * @return 连接是否成功
     */
    bool createConnection();

    /**
     * @brief 关闭 Redis 连接
     */
    void closeConnection();

    /**
     * @brief 执行数据点存储
     * @param data_point 数据点
     * @return 操作结果
     */
    RedisResult storeDataPointInternal(const DataPoint& data_point);

    /**
     * @brief 生成数据点键
     * @param source_id 数据源ID
     * @param node_id 节点ID
     * @return Redis 键
     */
    std::string generateDataPointKey(const std::string& source_id, const std::string& node_id);

    RedisConfig config_;                        ///< Redis 配置

    void* redis_context_;                       ///< hiredis 连接上下文
    std::atomic<bool> running_;                 ///< 运行标志
    std::thread worker_thread_;                 ///< 工作线程
    std::queue<AsyncTask> task_queue_;          ///< 任务队列
    mutable std::mutex queue_mutex_;            ///< 队列互斥锁
    std::condition_variable queue_cv_;          ///< 队列条件变量

    // 统计信息
    std::atomic<size_t> total_operations_;      ///< 总操作数
    std::atomic<size_t> successful_operations_; ///< 成功操作数
    std::atomic<size_t> failed_operations_;     ///< 失败操作数

    mutable std::mutex stats_mutex_;            ///< 统计信息互斥锁
};

} // namespace data_processor
