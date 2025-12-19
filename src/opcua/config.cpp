#include "config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace opcuaclient {

std::optional<OpcUaConfig> ConfigLoader::loadFromFiles(
    const std::filesystem::path& config_file_path,
    const std::filesystem::path& nodes_file_path) {

    // 加载主配置文件
    auto config = parseConfigFile(config_file_path);
    if (!config) {
        std::cerr << "Failed to parse config file: " << config_file_path << std::endl;
        return std::nullopt;
    }

    // 加载节点列表
    auto nodes = parseNodesFile(nodes_file_path);
    config->nodes = std::move(nodes);

    std::cout << "Configuration loaded successfully:" << std::endl;
    std::cout << "- Server URL: " << config->server_url << std::endl;
    std::cout << "- Security Mode: " << config->security_mode << std::endl;
    std::cout << "- Nodes to monitor: " << config->nodes.size() << std::endl;

    return config;
}

std::optional<OpcUaConfig> ConfigLoader::parseConfigFile(const std::filesystem::path& path) {
    OpcUaConfig config;

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
        if (key == "OPC_UA_URL") {
            config.server_url = value;
        } else if (key == "OPC_UA_SecurityMode") {
            config.security_mode = value;
        } else if (key == "ConnectionTimeout") {
            try {
                config.connection_timeout_ms = std::stoul(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid ConnectionTimeout value: " << value << std::endl;
            }
        } else if (key == "SessionTimeout") {
            try {
                config.session_timeout_ms = std::stoul(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid SessionTimeout value: " << value << std::endl;
            }
        } else if (key == "SubscriptionInterval") {
            try {
                config.subscription_interval_ms = std::stoul(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid SubscriptionInterval value: " << value << std::endl;
            }
        }
    }

    // 验证必要配置
    if (config.server_url.empty()) {
        std::cerr << "Server URL is required in config file" << std::endl;
        return std::nullopt;
    }

    return config;
}

std::vector<NodeConfig> ConfigLoader::parseNodesFile(const std::filesystem::path& path) {
    std::vector<NodeConfig> nodes;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Cannot open nodes file: " << path << std::endl;
        return nodes;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }

        NodeConfig node;
        node.node_id = line;
        node.sampling_interval_ms = 1000.0;  // 默认1秒采样间隔
        node.enabled = true;

        nodes.push_back(node);
    }

    return nodes;
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

} // namespace opcua
