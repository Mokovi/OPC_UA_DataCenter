#include "opcua_client/config.hpp"
#include "opcua_client/data_collector.hpp"
#include "opcua_client/client.hpp"
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
    std::cout << "Usage: " << program_name << " [config_file] [nodes_file]" << std::endl;
    std::cout << "  config_file: Path to configuration file (default: config)" << std::endl;
    std::cout << "  nodes_file:  Path to nodes file (default: nodes.txt)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << std::endl;
    std::cout << "  " << program_name << " config nodes.txt" << std::endl;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string config_file = "/root/project/dataCenter/config";
    std::string nodes_file = "/root/project/dataCenter/nodes.txt";

    if (argc >= 2) {
        config_file = argv[1];
    }
    if (argc >= 3) {
        nodes_file = argv[2];
    }

    if (argc > 3 || (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))) {
        showUsage(argv[0]);
        return 0;
    }

    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        std::cout << "OPC UA Data Collection System" << std::endl;
        std::cout << "Loading configuration from: " << config_file << std::endl;
        std::cout << "Loading nodes from: " << nodes_file << std::endl;
        std::cout << std::endl;

        // 加载配置
        auto config = opcuaclient::ConfigLoader::loadFromFiles(config_file, nodes_file);
        if (!config) {
            std::cerr << "Failed to load configuration" << std::endl;
            return 1;
        }

        // 创建数据采集器
        opcuaclient::DataCollector collector(*config);

        // 启动采集器
        if (!collector.start()) {
            std::cerr << "Failed to start data collector" << std::endl;
            return 1;
        }

        std::cout << "Data collection started. Press Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // 主循环：显示状态信息
        while (g_running) {
            auto state = collector.getClientState();
            std::cout << "\rClient State: ";
            switch (state) {
                case opcuaclient::ClientState::Disconnected:
                    std::cout << "Disconnected";
                    break;
                case opcuaclient::ClientState::Connecting:
                    std::cout << "Connecting...";
                    break;
                case opcuaclient::ClientState::Connected:
                    std::cout << "Connected";
                    break;
                case opcuaclient::ClientState::SessionActive:
                    std::cout << "Session Active";
                    break;
                case opcuaclient::ClientState::Error:
                    std::cout << "Error";
                    break;
            }
            std::cout << std::flush;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::cout << std::endl;

        // 停止采集器
        collector.stop();

        std::cout << "Data collection stopped. Goodbye!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
