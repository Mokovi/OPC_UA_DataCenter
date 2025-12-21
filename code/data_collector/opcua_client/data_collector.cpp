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
    // 初始化控制台处理器
    auto console_handler = std::make_shared<ConsoleDataHandler>();

    // 如果配置了 Kafka，初始化 Kafka 处理器
    std::shared_ptr<KafkaDataHandler> kafka_handler = nullptr;
    if (!config_.kafka_config.bootstrap_servers.empty() && !config_.kafka_config.topic.empty()) {
        try {
            auto kafka_producer = std::make_shared<kafka::LibrdKafkaProducer>(config_.kafka_config);
            kafka_handler = std::make_shared<KafkaDataHandler>(kafka_producer);
            std::cout << "Kafka producer initialized for topic: " << config_.kafka_config.topic << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize Kafka producer: " << e.what() << std::endl;
            std::cerr << "Continuing with console output only" << std::endl;
        }
    }

    // 创建复合处理器
    data_handler_ = std::make_shared<CompositeDataHandler>(console_handler, kafka_handler);
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

KafkaDataHandler::KafkaDataHandler(std::shared_ptr<kafka::IKafkaProducer> kafka_producer)
    : kafka_producer_(kafka_producer) {
}

void KafkaDataHandler::handleDataPoint(const DataPoint& data_point) {
    if (kafka_producer_ && kafka_producer_->send_data_point(data_point)) {
        ++success_count_;
    } else {
        ++failure_count_;
        std::cerr << "Failed to send data point to Kafka: " << data_point.node_id << std::endl;
    }
}

std::pair<size_t, size_t> KafkaDataHandler::getStats() const {
    return {success_count_, failure_count_};
}

CompositeDataHandler::CompositeDataHandler(std::shared_ptr<ConsoleDataHandler> console_handler,
                                          std::shared_ptr<KafkaDataHandler> kafka_handler)
    : console_handler_(console_handler)
    , kafka_handler_(kafka_handler) {
}

void CompositeDataHandler::handleDataPoint(const DataPoint& data_point) {
    if (console_handler_) {
        console_handler_->handleDataPoint(data_point);
    }
    if (kafka_handler_) {
        kafka_handler_->handleDataPoint(data_point);
    }
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

void DataCollector::setKafkaHandler(std::shared_ptr<KafkaDataHandler> kafka_handler) {
    // 获取当前的复合处理器
    auto composite_handler = std::dynamic_pointer_cast<CompositeDataHandler>(data_handler_);
    if (composite_handler) {
        // 更新 Kafka 处理器
        auto console_handler = std::dynamic_pointer_cast<ConsoleDataHandler>(composite_handler->getConsoleHandler());
        data_handler_ = std::make_shared<CompositeDataHandler>(console_handler, kafka_handler);
    } else {
        // 如果当前不是复合处理器，创建一个新的
        auto console_handler = std::make_shared<ConsoleDataHandler>();
        data_handler_ = std::make_shared<CompositeDataHandler>(console_handler, kafka_handler);
    }

    if (client_) {
        std::cout << "Warning: Kafka handler changed while client is running" << std::endl;
    }
}

} // namespace opcua
