#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include "../kafka_producer/kafka_producer.hpp"

namespace opcuaclient {

/**
 * @brief OPC UA 节点配置
 */
struct NodeConfig {
    std::string node_id;           ///< OPC UA 节点ID (如 "Sim.Device1.Test1")
    double sampling_interval_ms;   ///< 采样间隔 (毫秒)
    std::optional<double> deadband_absolute;  ///< 绝对死区值
    std::optional<double> deadband_relative;  ///< 相对死区百分比
    bool enabled;                  ///< 是否启用该节点采集

    NodeConfig() : sampling_interval_ms(1000.0), enabled(true) {}
};

/**
 * @brief OPC UA 客户端配置
 */
struct OpcUaConfig {
    std::string server_url;        ///< OPC UA 服务器URL
    std::string security_mode;     ///< 安全模式 ("None", "Sign", "SignAndEncrypt")
    uint32_t connection_timeout_ms; ///< 连接超时时间 (毫秒)
    uint32_t session_timeout_ms;   ///< 会话超时时间 (毫秒)
    uint32_t subscription_interval_ms; ///< 订阅发布间隔 (毫秒)
    std::vector<NodeConfig> nodes; ///< 要采集的节点列表

    // Kafka 配置
    kafka::KafkaConfig kafka_config; ///< Kafka 生产者配置

    OpcUaConfig() :
        connection_timeout_ms(5000),
        session_timeout_ms(30000),
        subscription_interval_ms(1000) {}
};

/**
 * @brief 配置加载器
 */
class ConfigLoader {
public:
    /**
     * @brief 从文件加载配置
     * @param config_file_path 配置文件路径
     * @param nodes_file_path 节点列表文件路径
     * @return 加载的配置，如果失败返回std::nullopt
     */
    static std::optional<OpcUaConfig> loadFromFiles(
        const std::filesystem::path& config_file_path,
        const std::filesystem::path& nodes_file_path);

private:
    /**
     * @brief 解析配置文件
     */
    static std::optional<OpcUaConfig> parseConfigFile(const std::filesystem::path& path);

    /**
     * @brief 解析节点列表文件
     */
    static std::vector<NodeConfig> parseNodesFile(const std::filesystem::path& path);

    /**
     * @brief 去除字符串首尾空白字符
     */
    static std::string trim(const std::string& str);
};

} // namespace opcua
