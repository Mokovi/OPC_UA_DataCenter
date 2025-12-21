#pragma once

#include "../opcua_client/data_point.hpp"
#include <memory>
#include <string>
#include <vector>

namespace kafka {

/**
 * @brief Kafka 生产者配置
 */
struct KafkaConfig {
    std::vector<std::string> bootstrap_servers;  ///< Kafka 服务器地址列表
    std::string topic;                          ///< 目标主题
    std::string client_id;                      ///< 客户端ID
    int acks = 1;                              ///< 确认模式 (0=不等待, 1=等待leader, -1=等待所有副本)
    int retries = 3;                           ///< 重试次数
    int batch_size = 16384;                    ///< 批量大小
    int linger_ms = 5;                         ///< 延迟发送时间
    int max_in_flight_requests_per_connection = 5;  ///< 每个连接最大并发请求数

    /**
     * @brief 获取服务器地址字符串 (逗号分隔)
     */
    std::string get_bootstrap_servers_string() const;
};

/**
 * @brief Kafka 消息生产者接口
 */
class IKafkaProducer {
public:
    virtual ~IKafkaProducer() = default;

    /**
     * @brief 发送数据点到 Kafka
     * @param data_point 数据点
     * @return 发送是否成功
     */
    virtual bool send_data_point(const opcuaclient::DataPoint& data_point) = 0;

    /**
     * @brief 发送批量数据点到 Kafka
     * @param data_points 数据点列表
     * @return 发送成功的数据点数量
     */
    virtual size_t send_data_points(const std::vector<opcuaclient::DataPoint>& data_points) = 0;

    /**
     * @brief 刷新缓冲区，确保所有消息都被发送
     * @param timeout_ms 超时时间(毫秒)
     * @return 操作是否成功
     */
    virtual bool flush(int timeout_ms = 10000) = 0;

    /**
     * @brief 获取生产者状态
     * @return 状态描述字符串
     */
    virtual std::string get_status() const = 0;
};

/**
 * @brief 基于 librdkafka 的 Kafka 生产者实现
 */
class LibrdKafkaProducer : public IKafkaProducer {
public:
    /**
     * @brief 构造函数
     * @param config Kafka 配置
     */
    explicit LibrdKafkaProducer(const KafkaConfig& config);

    /**
     * @brief 析构函数
     */
    ~LibrdKafkaProducer() override;

    /**
     * @brief 发送数据点到 Kafka
     * @param data_point 数据点
     * @return 发送是否成功
     */
    bool send_data_point(const opcuaclient::DataPoint& data_point) override;

    /**
     * @brief 发送批量数据点到 Kafka
     * @param data_points 数据点列表
     * @return 发送成功的数据点数量
     */
    size_t send_data_points(const std::vector<opcuaclient::DataPoint>& data_points) override;

    /**
     * @brief 刷新缓冲区，确保所有消息都被发送
     * @param timeout_ms 超时时间(毫秒)
     * @return 操作是否成功
     */
    bool flush(int timeout_ms = 10000) override;

    /**
     * @brief 获取生产者状态
     * @return 状态描述字符串
     */
    std::string get_status() const override;

private:
    /**
     * @brief 初始化 Kafka 生产者
     * @return 初始化是否成功
     */
    bool initialize_producer();

    /**
     * @brief 将数据点序列化为 JSON 字符串
     * @param data_point 数据点
     * @return JSON 字符串
     */
    std::string serialize_data_point(const opcuaclient::DataPoint& data_point) const;

    KafkaConfig config_;                       ///< Kafka 配置
    void* producer_handle_;                   ///< librdkafka 生产者句柄
    bool initialized_;                        ///< 是否已初始化
};

} // namespace kafka
