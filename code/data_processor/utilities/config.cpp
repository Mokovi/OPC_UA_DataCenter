#include "config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace data_processor {

std::string KafkaConsumerConfig::get_bootstrap_servers_string() const {
    if (bootstrap_servers.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < bootstrap_servers.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << bootstrap_servers[i];
    }
    return oss.str();
}

std::optional<DataProcessorConfig> ConfigLoader::loadFromFile(
    const std::filesystem::path& config_file_path) {

    // 加载配置文件
    auto config = parseConfigFile(config_file_path);
    if (!config) {
        std::cerr << "Failed to parse config file: " << config_file_path << std::endl;
        return std::nullopt;
    }

    std::cout << "Data processor configuration loaded successfully:" << std::endl;
    std::cout << "- Kafka servers: " << config->kafka_config.get_bootstrap_servers_string() << std::endl;
    std::cout << "- Kafka topic: " << config->kafka_config.topic << std::endl;
    std::cout << "- Kafka group: " << config->kafka_config.group_id << std::endl;
    std::cout << "- Redis: " << config->redis_config.host << ":" << config->redis_config.port << std::endl;
    std::cout << "- MySQL: " << config->mysql_config.host << ":" << config->mysql_config.port
              << "/" << config->mysql_config.database << std::endl;

    return config;
}

std::optional<DataProcessorConfig> ConfigLoader::parseConfigFile(const std::filesystem::path& path) {
    DataProcessorConfig config;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Cannot open config file: " << path << std::endl;
        return std::nullopt;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 解析键值对
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        // 移除方括号（如果有）
        if (!key.empty() && key[0] == '[' && key.back() == ']') {
            key = key.substr(1, key.size() - 2);
        }

        // 解析配置项
        if (key == "KafkaBootstrapServers") {
            // 支持逗号分隔的服务器列表
            std::istringstream iss(value);
            std::string server;
            while (std::getline(iss, server, ',')) {
                server = trim(server);
                if (!server.empty()) {
                    config.kafka_config.bootstrap_servers.push_back(server);
                }
            }
        } else if (key == "KafkaTopic") {
            config.kafka_config.topic = value;
        } else if (key == "KafkaGroupId") {
            config.kafka_config.group_id = value;
        } else if (key == "KafkaClientId") {
            config.kafka_config.client_id = value;
        } else if (key == "KafkaAutoCommit") {
            config.kafka_config.enable_auto_commit = (value == "true" || value == "1");
        } else if (key == "KafkaAutoCommitInterval") {
            try {
                config.kafka_config.auto_commit_interval_ms = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid KafkaAutoCommitInterval value: " << value << std::endl;
            }
        } else if (key == "KafkaSessionTimeout") {
            try {
                config.kafka_config.session_timeout_ms = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid KafkaSessionTimeout value: " << value << std::endl;
            }
        } else if (key == "RedisHost") {
            config.redis_config.host = value;
        } else if (key == "RedisPort") {
            try {
                config.redis_config.port = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid RedisPort value: " << value << std::endl;
            }
        } else if (key == "RedisPassword") {
            config.redis_config.password = value;
        } else if (key == "RedisConnectionTimeout") {
            try {
                config.redis_config.connection_timeout_ms = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid RedisConnectionTimeout value: " << value << std::endl;
            }
        } else if (key == "RedisConnectionPoolSize") {
            try {
                config.redis_config.connection_pool_size = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid RedisConnectionPoolSize value: " << value << std::endl;
            }
        } else if (key == "MySQLHost") {
            config.mysql_config.host = value;
        } else if (key == "MySQLPort") {
            try {
                config.mysql_config.port = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid MySQLPort value: " << value << std::endl;
            }
        } else if (key == "MySQLUsername") {
            config.mysql_config.username = value;
        } else if (key == "MySQLPassword") {
            config.mysql_config.password = value;
        } else if (key == "MySQLDatabase") {
            config.mysql_config.database = value;
        } else if (key == "MySQLCharset") {
            config.mysql_config.charset = value;
        } else if (key == "MySQLConnectionTimeout") {
            try {
                config.mysql_config.connection_timeout_sec = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid MySQLConnectionTimeout value: " << value << std::endl;
            }
        } else if (key == "EnableConsoleOutput") {
            config.enable_console_output = (value == "true" || value == "1");
        } else if (key == "ProcessingThreads") {
            try {
                config.processing_threads = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid ProcessingThreads value: " << value << std::endl;
            }
        } else if (key == "MaxBatchSize") {
            try {
                config.max_batch_size = std::stoi(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid MaxBatchSize value: " << value << std::endl;
            }
        }
    }

    // 验证必要配置
    if (config.kafka_config.bootstrap_servers.empty()) {
        std::cerr << "Kafka bootstrap servers is required" << std::endl;
        return std::nullopt;
    }

    if (config.kafka_config.topic.empty()) {
        std::cerr << "Kafka topic is required" << std::endl;
        return std::nullopt;
    }

    if (config.kafka_config.group_id.empty()) {
        std::cerr << "Kafka group ID is required" << std::endl;
        return std::nullopt;
    }

    return config;
}

std::string ConfigLoader::trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        ++start;
    }

    auto end = str.end();
    do {
        --end;
    } while (end != start && std::isspace(*end));

    return std::string(start, end + 1);
}

} // namespace data_processor
