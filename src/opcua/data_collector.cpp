#include "data_collector.hpp"
#include "client.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

namespace opcuaclient {

void ConsoleDataHandler::handleDataPoint(const DataPoint& data_point) {
    if (!verbose_) {
        return;
    }

    // 格式化时间戳
    auto time_t = std::chrono::system_clock::to_time_t(data_point.ingest_timestamp);
    std::tm tm = *std::localtime(&time_t);

    std::cout << "["
              << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << "] ";

    if (data_point.hasError()) {
        std::cout << "ERROR - Node: " << data_point.node_id
                  << ", Error: " << data_point.error_message.value() << std::endl;
        return;
    }

    std::cout << "DATA - Node: " << data_point.node_id
              << ", Value: " << data_point.valueAsString()
              << ", Quality: " << (data_point.isGood() ? "Good" : "Bad")
              << std::endl;
}

DataCollector::DataCollector(const OpcUaConfig& config)
    : config_(config) {
    // 默认使用控制台数据处理器
    data_handler_ = std::make_shared<ConsoleDataHandler>();
}

DataCollector::~DataCollector() {
    stop();
}

bool DataCollector::start() {
    if (client_) {
        std::cout << "Data collector is already running" << std::endl;
        return true;
    }

    try {
        // 创建OPC UA客户端
        client_ = std::make_unique<OpcUaClient>(config_, data_handler_);

        // 启动客户端
        if (client_->start()) {
            std::cout << "Data collector started successfully" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to start OPC UA client" << std::endl;
            client_.reset();
            return false;
        }

    } catch (const std::exception& e) {
        std::cerr << "Failed to start data collector: " << e.what() << std::endl;
        client_.reset();
        return false;
    }

    // This should never be reached, but added for completeness
    return false;
}

void DataCollector::stop() {
    if (client_) {
        client_->stop();
        client_.reset();
        std::cout << "Data collector stopped" << std::endl;
    }
}

opcuaclient::ClientState DataCollector::getClientState() const {
    return client_ ? client_->getState() : ClientState::Disconnected;
}

void DataCollector::setDataHandler(std::shared_ptr<IDataPointHandler> handler) {
    data_handler_ = handler;
    // 如果客户端正在运行，需要重新设置处理器（这里简化处理）
    if (client_) {
        std::cout << "Warning: Data handler changed while client is running" << std::endl;
    }
}

} // namespace opcua
