#pragma once

#include "../config.hpp"
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
