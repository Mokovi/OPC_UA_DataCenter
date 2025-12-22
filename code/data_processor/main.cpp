#include "utilities/config.hpp"
#include "kafka_consumer/kafka_consumer.hpp"
#include "redis_client/redis_client.hpp"
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
[[maybe_unused]] void signalHandler(int signal) {
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

        // 初始化 Redis 客户端
        std::shared_ptr<data_processor::IRedisClient> redis_client = nullptr;
        try {
            redis_client = std::make_shared<data_processor::HiredisAsyncClient>(config->redis_config);
            if (redis_client->start()) {
                std::cout << "Redis client started successfully" << std::endl;
            } else {
                std::cerr << "Failed to start Redis client" << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize Redis client: " << e.what() << std::endl;
            return 1;
        }

        // 创建消息处理器
        auto console_handler = std::make_shared<data_processor::ConsoleMessageHandler>();
        auto redis_handler = std::make_shared<data_processor::RedisDataHandler>(redis_client);

        // 使用复合处理器同时输出到控制台和 Redis
        auto message_handler = std::make_shared<data_processor::CompositeMessageHandler>(
            console_handler, redis_handler);

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
            std::cout << "\rConsumer Status: " << kafka_consumer->getStatus();
            std::cout << " | Redis Status: " << redis_client->getStatus();

            auto redis_handler_ptr = std::dynamic_pointer_cast<data_processor::RedisDataHandler>(redis_handler);
            if (redis_handler_ptr) {
                auto [success, failure] = redis_handler_ptr->getStats();
                std::cout << " | Redis Stats: " << success << "/" << (success + failure) << " stored";
            }

            std::cout << std::flush;
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
