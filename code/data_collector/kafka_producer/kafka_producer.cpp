#include "kafka_producer.hpp"
#include <librdkafka/rdkafkacpp.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace kafka {

std::string KafkaConfig::get_bootstrap_servers_string() const {
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

class DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message& message) override {
        if (message.err()) {
            std::cerr << "Message delivery failed: " << message.errstr() << std::endl;
        } else {
            // 消息发送成功，可以在这里添加统计信息
        }
    }
};

class EventCb : public RdKafka::EventCb {
public:
    void event_cb(RdKafka::Event& event) override {
        switch (event.type()) {
            case RdKafka::Event::EVENT_ERROR:
                std::cerr << "Kafka error: " << RdKafka::err2str(event.err())
                          << ": " << event.str() << std::endl;
                break;
            case RdKafka::Event::EVENT_STATS:
                // 统计信息，可以用于监控
                break;
            case RdKafka::Event::EVENT_LOG:
                // 日志信息
                break;
            default:
                break;
        }
    }
};

LibrdKafkaProducer::LibrdKafkaProducer(const KafkaConfig& config)
    : config_(config)
    , producer_handle_(nullptr)
    , initialized_(false) {
    if (!initialize_producer()) {
        throw std::runtime_error("Failed to initialize Kafka producer");
    }
}

LibrdKafkaProducer::~LibrdKafkaProducer() {
    if (producer_handle_) {
        // 刷新缓冲区
        flush(5000);

        // 销毁生产者
        delete static_cast<RdKafka::Producer*>(producer_handle_);
        producer_handle_ = nullptr;
    }

    // 清理 librdkafka 资源
    RdKafka::wait_destroyed(5000);
}

bool LibrdKafkaProducer::initialize_producer() {
    try {
        // 设置全局配置
        RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

        // 基本配置
        std::string errstr;
        if (conf->set("bootstrap.servers", config_.get_bootstrap_servers_string(), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set bootstrap.servers: " << errstr << std::endl;
            delete conf;
            return false;
        }

        if (!config_.client_id.empty()) {
            if (conf->set("client.id", config_.client_id, errstr) != RdKafka::Conf::CONF_OK) {
                std::cerr << "Failed to set client.id: " << errstr << std::endl;
                delete conf;
                return false;
            }
        }

        // 确认模式
        std::string acks_str = std::to_string(config_.acks);
        if (conf->set("acks", acks_str, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set acks: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 重试配置
        if (conf->set("retries", std::to_string(config_.retries), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set retries: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 批量配置
        if (conf->set("batch.size", std::to_string(config_.batch_size), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set batch.size: " << errstr << std::endl;
            delete conf;
            return false;
        }

        if (conf->set("linger.ms", std::to_string(config_.linger_ms), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set linger.ms: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 并发请求配置
        if (conf->set("max.in.flight.requests.per.connection",
                      std::to_string(config_.max_in_flight_requests_per_connection), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set max.in.flight.requests.per.connection: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 设置回调
        DeliveryReportCb* dr_cb = new DeliveryReportCb();
        if (conf->set("dr_cb", dr_cb, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set delivery report callback: " << errstr << std::endl;
            delete dr_cb;
            delete conf;
            return false;
        }

        EventCb* event_cb = new EventCb();
        if (conf->set("event_cb", event_cb, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set event callback: " << errstr << std::endl;
            delete event_cb;
            delete dr_cb;
            delete conf;
            return false;
        }

        // 创建生产者
        RdKafka::Producer* producer = RdKafka::Producer::create(conf, errstr);
        if (!producer) {
            std::cerr << "Failed to create producer: " << errstr << std::endl;
            delete event_cb;
            delete dr_cb;
            delete conf;
            return false;
        }

        // 保存句柄
        producer_handle_ = producer;
        initialized_ = true;

        std::cout << "Kafka producer initialized successfully" << std::endl;
        std::cout << "Bootstrap servers: " << config_.get_bootstrap_servers_string() << std::endl;
        std::cout << "Topic: " << config_.topic << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during producer initialization: " << e.what() << std::endl;
        return false;
    }
}

bool LibrdKafkaProducer::send_data_point(const opcuaclient::DataPoint& data_point) {
    if (!initialized_ || !producer_handle_) {
        std::cerr << "Producer not initialized" << std::endl;
        return false;
    }

    try {
        RdKafka::Producer* producer = static_cast<RdKafka::Producer*>(producer_handle_);

        // 序列化数据点
        std::string payload = serialize_data_point(data_point);

        // 创建消息
        RdKafka::ErrorCode err = producer->produce(
            config_.topic,                           // topic name
            RdKafka::Topic::PARTITION_UA,           // partition (unassigned)
            RdKafka::Producer::RK_MSG_COPY,         // copy payload
            const_cast<char*>(payload.data()),      // payload
            payload.size(),                         // payload size
            nullptr,                                // key
            0,                                      // key size
            0,                                      // timestamp (0 = not available)
            nullptr                                 // headers
        );

        if (err != RdKafka::ERR_NO_ERROR) {
            std::cerr << "Failed to produce message: " << RdKafka::err2str(err) << std::endl;
            return false;
        }

        // 轮询处理事件
        producer->poll(0);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during message production: " << e.what() << std::endl;
        return false;
    }
}

size_t LibrdKafkaProducer::send_data_points(const std::vector<opcuaclient::DataPoint>& data_points) {
    size_t success_count = 0;

    for (const auto& data_point : data_points) {
        if (send_data_point(data_point)) {
            ++success_count;
        }
    }

    return success_count;
}

bool LibrdKafkaProducer::flush(int timeout_ms) {
    if (!initialized_ || !producer_handle_) {
        return false;
    }

    RdKafka::Producer* producer = static_cast<RdKafka::Producer*>(producer_handle_);
    return producer->flush(timeout_ms) == RdKafka::ERR_NO_ERROR;
}

std::string LibrdKafkaProducer::get_status() const {
    if (!initialized_) {
        return "Not initialized";
    }

    if (!producer_handle_) {
        return "No producer handle";
    }

    // 这里可以添加更多状态信息，比如队列长度、出错统计等
    return "Active";
}

std::string LibrdKafkaProducer::serialize_data_point(const opcuaclient::DataPoint& data_point) const {
    // 简单的 JSON 序列化
    std::ostringstream oss;

    // 时间戳格式化
    auto time_t = std::chrono::system_clock::to_time_t(data_point.ingest_timestamp);
    std::tm tm = *std::localtime(&time_t);
    char time_buffer[32];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%S", &tm);

    oss << "{";
    oss << "\"source_id\":\"" << data_point.source_id << "\",";
    oss << "\"node_id\":\"" << data_point.node_id << "\",";
    oss << "\"value\":\"" << data_point.valueAsString() << "\",";
    oss << "\"device_timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
        data_point.device_timestamp.time_since_epoch()).count() << ",";
    oss << "\"ingest_timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
        data_point.ingest_timestamp.time_since_epoch()).count() << ",";
    oss << "\"quality\":" << static_cast<int>(data_point.quality);

    if (data_point.hasError()) {
        oss << ",\"error_message\":\"" << data_point.error_message.value() << "\"";
    }

    oss << "}";

    return oss.str();
}

} // namespace kafka
