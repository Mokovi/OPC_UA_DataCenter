#pragma once

#include "../utilities/config.hpp"
#include "../redis_client/redis_client.hpp"
#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <functional>

namespace data_processor {

/**
 * @brief Kafka 消息处理器接口
 */
class IKafkaMessageHandler {
public:
    virtual ~IKafkaMessageHandler() = default;

    /**
     * @brief 处理接收到的消息
     * @param topic 主题
     * @param partition 分区
     * @param offset 偏移量
     * @param key 消息键
     * @param payload 消息内容
     */
    virtual void handleMessage(const std::string& topic, int32_t partition,
                              int64_t offset, const std::string& key,
                              const std::string& payload) = 0;
};

/**
 * @brief 控制台消息处理器
 * 将接收到的消息打印到控制台
 */
class ConsoleMessageHandler : public IKafkaMessageHandler {
public:
    /**
     * @brief 处理接收到的消息
     */
    void handleMessage(const std::string& topic, int32_t partition,
                      int64_t offset, const std::string& key,
                      const std::string& payload) override;
};

/**
 * @brief Redis 数据处理器
 * 将 Kafka 消息解析后存储到 Redis
 */
class RedisDataHandler : public IKafkaMessageHandler {
public:
    /**
     * @brief 构造函数
     * @param redis_client Redis 客户端实例
     */
    explicit RedisDataHandler(std::shared_ptr<IRedisClient> redis_client);

    /**
     * @brief 处理接收到的消息
     */
    void handleMessage(const std::string& topic, int32_t partition,
                      int64_t offset, const std::string& key,
                      const std::string& payload) override;

    /**
     * @brief 获取处理统计信息
     * @return 处理成功和失败的数量
     */
    std::pair<size_t, size_t> getStats() const;

    /**
     * @brief 刷新缓冲区，确保所有数据都被写入 Redis
     */
    void flush();

private:
    /**
     * @brief 解析 JSON 消息并提取数据点信息
     * @param payload JSON 消息内容
     * @return 解析后的数据点信息
     */
    std::optional<DataPoint> parseDataPoint(const std::string& payload);

    std::shared_ptr<IRedisClient> redis_client_;      ///< Redis 客户端
    size_t success_count_ = 0;                        ///< 成功处理数量
    size_t failure_count_ = 0;                        ///< 失败处理数量
    mutable std::mutex stats_mutex_;                  ///< 统计信息互斥锁
};

/**
 * @brief 复合消息处理器
 * 支持同时将消息发送到多个处理器（如控制台和Redis）
 */
class CompositeMessageHandler : public IKafkaMessageHandler {
public:
    /**
     * @brief 构造函数
     * @param handler1 第一个消息处理器
     * @param handler2 第二个消息处理器
     */
    CompositeMessageHandler(std::shared_ptr<IKafkaMessageHandler> handler1,
                           std::shared_ptr<IKafkaMessageHandler> handler2);

    /**
     * @brief 处理接收到的消息
     */
    void handleMessage(const std::string& topic, int32_t partition,
                      int64_t offset, const std::string& key,
                      const std::string& payload) override;

private:
    std::shared_ptr<IKafkaMessageHandler> handler1_;  ///< 第一个处理器
    std::shared_ptr<IKafkaMessageHandler> handler2_;  ///< 第二个处理器
};

/**
 * @brief Kafka 消费者接口
 */
class IKafkaConsumer {
public:
    virtual ~IKafkaConsumer() = default;

    /**
     * @brief 启动消费者
     * @return 启动是否成功
     */
    virtual bool start() = 0;

    /**
     * @brief 停止消费者
     */
    virtual void stop() = 0;

    /**
     * @brief 获取消费者状态
     * @return 状态描述字符串
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief 设置消息处理器
     * @param handler 消息处理器
     */
    virtual void setMessageHandler(std::shared_ptr<IKafkaMessageHandler> handler) = 0;
};

/**
 * @brief 基于 librdkafka 的 Kafka 消费者实现
 */
class LibrdKafkaConsumer : public IKafkaConsumer {
public:
    /**
     * @brief 构造函数
     * @param config Kafka 消费者配置
     */
    explicit LibrdKafkaConsumer(const KafkaConsumerConfig& config);

    /**
     * @brief 析构函数
     */
    ~LibrdKafkaConsumer() override;

    /**
     * @brief 启动消费者
     * @return 启动是否成功
     */
    bool start() override;

    /**
     * @brief 停止消费者
     */
    void stop() override;

    /**
     * @brief 获取消费者状态
     * @return 状态描述字符串
     */
    std::string getStatus() const override;

    /**
     * @brief 设置消息处理器
     * @param handler 消息处理器
     */
    void setMessageHandler(std::shared_ptr<IKafkaMessageHandler> handler) override;

private:
    /**
     * @brief 初始化 Kafka 消费者
     * @return 初始化是否成功
     */
    bool initializeConsumer();

    /**
     * @brief 消费者工作线程
     */
    void consumerThread();

    /**
     * @brief 处理消息
     * @param message Kafka 消息
     */
    void processMessage(RdKafka::Message* message);

    KafkaConsumerConfig config_;                       ///< Kafka 配置
    std::shared_ptr<IKafkaMessageHandler> message_handler_; ///< 消息处理器

    void* consumer_handle_;                           ///< librdkafka 消费者句柄
    std::atomic<bool> running_;                       ///< 运行标志
    std::thread consumer_thread_;                     ///< 消费者线程
    bool initialized_;                                ///< 是否已初始化
};

} // namespace data_processor
