#include "config.hpp"
#include "kafka_consumer/kafka_consumer.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

namespace {

// 全局运行标志
std::atomic<bool> g_running{true};

/**
 * @brief 信号处理函数
 */
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_running = false;
    }
}

/**
 * @brief 显示使用帮助
 */
void showUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [config_file]" << std::endl;
    std::cout << "  config_file: Path to configuration file (default: config)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << std::endl;
    std::cout << "  " << program_name << " config" << std::endl;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string config_file = "/root/project/dataCenter/config";

    if (argc >= 2) {
        config_file = argv[1];
    }

    if (argc > 2 || (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))) {
        showUsage(argv[0]);
        return 0;
    }

    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        std::cout << "Data Processing System" << std::endl;
        std::cout << "Loading configuration from: " << config_file << std::endl;
        std::cout << std::endl;

        // 加载配置
        auto config = data_processor::ConfigLoader::loadFromFile(config_file);
        if (!config) {
            std::cerr << "Failed to load configuration" << std::endl;
            return 1;
        }

        // 创建 Kafka 消费者
        auto kafka_consumer = std::make_shared<data_processor::LibrdKafkaConsumer>(config->kafka_config);

        // 设置消息处理器（控制台输出）
        auto message_handler = std::make_shared<data_processor::ConsoleMessageHandler>();
        kafka_consumer->setMessageHandler(message_handler);

        // 启动消费者
        if (!kafka_consumer->start()) {
            std::cerr << "Failed to start Kafka consumer" << std::endl;
            return 1;
        }

        std::cout << "Data processor started. Press Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // 主循环：显示状态信息
        while (g_running) {
            std::cout << "\rConsumer Status: " << kafka_consumer->getStatus() << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << std::endl;

        // 停止消费者
        kafka_consumer->stop();

        std::cout << "Data processor stopped. Goodbye!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
