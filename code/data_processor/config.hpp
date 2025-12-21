#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace data_processor {

/**
 * @brief Kafka 消费者配置
 */
struct KafkaConsumerConfig {
    std::vector<std::string> bootstrap_servers;  ///< Kafka 服务器地址列表
    std::string topic;                          ///< 消费主题
    std::string group_id;                       ///< 消费者组ID
    std::string client_id;                      ///< 客户端ID
    bool enable_auto_commit = true;             ///< 是否自动提交offset
    int auto_commit_interval_ms = 5000;         ///< 自动提交间隔
    int session_timeout_ms = 30000;             ///< 会话超时时间
    int max_poll_interval_ms = 300000;          ///< 最大轮询间隔
    std::string auto_offset_reset = "latest";   ///< 自动偏移重置策略

    /**
     * @brief 获取服务器地址字符串 (逗号分隔)
     */
    std::string get_bootstrap_servers_string() const;
};

/**
 * @brief Redis 配置
 */
struct RedisConfig {
    std::string host = "localhost";             ///< Redis 服务器地址
    int port = 6379;                           ///< Redis 端口
    std::string password;                       ///< Redis 密码
    int connection_timeout_ms = 5000;          ///< 连接超时时间
    int connection_pool_size = 10;             ///< 连接池大小
    int db_index = 0;                          ///< 数据库索引
};

/**
 * @brief MySQL 配置
 */
struct MySQLConfig {
    std::string host = "localhost";             ///< MySQL 服务器地址
    int port = 3306;                           ///< MySQL 端口
    std::string username;                       ///< 用户名
    std::string password;                       ///< 密码
    std::string database;                       ///< 数据库名
    std::string charset = "utf8mb4";            ///< 字符集
    int connection_timeout_sec = 30;           ///< 连接超时时间
    int max_connections = 10;                  ///< 最大连接数
    bool enable_reconnect = true;              ///< 是否启用重连
    int reconnect_attempts = 3;                ///< 重连尝试次数
};

/**
 * @brief 数据处理器配置
 */
struct DataProcessorConfig {
    KafkaConsumerConfig kafka_config;          ///< Kafka 消费者配置
    RedisConfig redis_config;                  ///< Redis 配置
    MySQLConfig mysql_config;                  ///< MySQL 配置

    // 处理器通用配置
    bool enable_console_output = true;         ///< 是否启用控制台输出
    int processing_threads = 4;                ///< 处理线程数
    int max_batch_size = 100;                  ///< 批量处理大小
};

/**
 * @brief 数据处理器配置加载器
 */
class ConfigLoader {
public:
    /**
     * @brief 从文件加载配置
     * @param config_file_path 配置文件路径
     * @return 加载的配置，如果失败返回std::nullopt
     */
    static std::optional<DataProcessorConfig> loadFromFile(
        const std::filesystem::path& config_file_path);

private:
    /**
     * @brief 解析配置文件
     */
    static std::optional<DataProcessorConfig> parseConfigFile(const std::filesystem::path& path);

    /**
     * @brief 去除字符串首尾空白字符
     */
    static std::string trim(const std::string& str);
};

} // namespace data_processor
