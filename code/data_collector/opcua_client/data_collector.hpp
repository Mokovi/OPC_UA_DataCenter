#pragma once

#include "data_point.hpp"
#include "client.hpp"
#include "../kafka_producer/kafka_producer.hpp"
#include <iostream>
#include <memory>

namespace opcuaclient {

/**
 * @brief 控制台数据处理器
 * 用于将数据点输出到控制台的简单实现
 */
class ConsoleDataHandler : public IDataPointHandler {
public:
    /**
     * @brief 处理接收到的数据点
     * @param data_point 数据点
     */
    void handleDataPoint(const DataPoint& data_point) override;

    /**
     * @brief 设置是否显示详细信息
     * @param verbose 是否详细输出
     */
    void setVerbose(bool verbose) { verbose_ = verbose; }

private:
    bool verbose_ = true;  ///< 是否详细输出
};

/**
 * @brief Kafka 数据处理器
 * 用于将数据点发送到 Kafka 的实现
 */
class KafkaDataHandler : public IDataPointHandler {
public:
    /**
     * @brief 构造函数
     * @param kafka_producer Kafka 生产者实例
     */
    explicit KafkaDataHandler(std::shared_ptr<kafka::IKafkaProducer> kafka_producer);

    /**
     * @brief 处理接收到的数据点
     * @param data_point 数据点
     */
    void handleDataPoint(const DataPoint& data_point) override;

    /**
     * @brief 获取发送统计信息
     * @return 发送成功和失败的次数
     */
    std::pair<size_t, size_t> getStats() const;

private:
    std::shared_ptr<kafka::IKafkaProducer> kafka_producer_;  ///< Kafka 生产者
    size_t success_count_ = 0;                               ///< 发送成功次数
    size_t failure_count_ = 0;                               ///< 发送失败次数
};

/**
 * @brief 复合数据处理器
 * 支持同时输出到控制台和发送到 Kafka
 */
class CompositeDataHandler : public IDataPointHandler {
public:
    /**
     * @brief 构造函数
     * @param console_handler 控制台处理器（可为空）
     * @param kafka_handler Kafka 处理器（可为空）
     */
    CompositeDataHandler(std::shared_ptr<ConsoleDataHandler> console_handler,
                        std::shared_ptr<KafkaDataHandler> kafka_handler);

    /**
     * @brief 处理接收到的数据点
     * @param data_point 数据点
     */
    void handleDataPoint(const DataPoint& data_point) override;

    /**
     * @brief 获取控制台处理器
     * @return 控制台处理器实例
     */
    std::shared_ptr<ConsoleDataHandler> getConsoleHandler() const { return console_handler_; }

    /**
     * @brief 获取 Kafka 处理器
     * @return Kafka 处理器实例
     */
    std::shared_ptr<KafkaDataHandler> getKafkaHandler() const { return kafka_handler_; }

private:
    std::shared_ptr<ConsoleDataHandler> console_handler_;  ///< 控制台处理器
    std::shared_ptr<KafkaDataHandler> kafka_handler_;     ///< Kafka 处理器
};

/**
 * @brief 数据采集器
 * 封装OPC UA客户端和数据处理器的完整采集流程
 */
class DataCollector {
public:
    /**
     * @brief 构造函数
     * @param config OPC UA配置
     */
    explicit DataCollector(const OpcUaConfig& config);

    /**
     * @brief 析构函数
     */
    ~DataCollector();

    /**
     * @brief 启动数据采集
     * @return 启动是否成功
     */
    bool start();

    /**
     * @brief 停止数据采集
     */
    void stop();

    /**
     * @brief 获取客户端状态
     */
    ClientState getClientState() const;

    /**
     * @brief 设置数据处理器
     * @param handler 数据处理器
     */
    void setDataHandler(std::shared_ptr<IDataPointHandler> handler);

    /**
     * @brief 设置 Kafka 处理器
     * @param kafka_handler Kafka 数据处理器
     */
    void setKafkaHandler(std::shared_ptr<KafkaDataHandler> kafka_handler);

private:
    const OpcUaConfig& config_;                          ///< 配置引用
    std::unique_ptr<OpcUaClient> client_;                ///< OPC UA客户端
    std::shared_ptr<IDataPointHandler> data_handler_;    ///< 数据处理器
};

} // namespace opcua
