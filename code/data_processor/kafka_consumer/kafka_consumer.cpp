#include "kafka_consumer.hpp"
#include <librdkafka/rdkafkacpp.h>
#include <iostream>
#include <chrono>
#include <thread>
#include "../utilities/json_parser.hpp"

namespace data_processor {

void ConsoleMessageHandler::handleMessage(const std::string& topic, int32_t partition,
                                         int64_t offset, const std::string& key,
                                         const std::string& payload) {
    // 格式化时间戳
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);

    std::cout << "["
              << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << "] ";

    std::cout << "KAFKA - Topic: " << topic
              << ", Partition: " << partition
              << ", Offset: " << offset;

    if (!key.empty()) {
        std::cout << ", Key: " << key;
    }

    std::cout << ", Payload: " << payload << std::endl;
}

RedisDataHandler::RedisDataHandler(std::shared_ptr<IRedisClient> redis_client)
    : redis_client_(redis_client) {
}

void RedisDataHandler::handleMessage(const std::string& /*topic*/, int32_t /*partition*/,
                                   int64_t offset, const std::string& /*key*/,
                                   const std::string& payload) {
    // 解析 JSON 消息
    auto data_point = parseDataPoint(payload);
    if (!data_point) {
        failure_count_++;
        std::cerr << "Failed to parse data point from message at offset " << offset << std::endl;
        return;
    }

    // 异步存储到 Redis
    redis_client_->storeDataPointAsync(*data_point, [this, offset](RedisResult result) {
        if (result == RedisResult::Success) {
            success_count_++;
        } else {
            failure_count_++;
            std::cerr << "Failed to store data point at offset " << offset
                      << " to Redis, error code: " << static_cast<int>(result) << std::endl;
        }
    });
}

std::pair<size_t, size_t> RedisDataHandler::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {success_count_, failure_count_};
}

void RedisDataHandler::flush() {
    // Redis 客户端是异步的，这里可以等待所有待处理的操作完成
    std::cout << "Flushing Redis data handler..." << std::endl;
    // 实际实现可能需要等待异步操作完成
}

std::optional<DataPoint> RedisDataHandler::parseDataPoint(const std::string& payload) {
    return JsonMessageParser::parseDataPoint(payload);
}

CompositeMessageHandler::CompositeMessageHandler(std::shared_ptr<IKafkaMessageHandler> handler1,
                                               std::shared_ptr<IKafkaMessageHandler> handler2)
    : handler1_(handler1)
    , handler2_(handler2) {
}

void CompositeMessageHandler::handleMessage(const std::string& topic, int32_t partition,
                                          int64_t offset, const std::string& key,
                                          const std::string& payload) {
    // 同时发送到两个处理器
    if (handler1_) {
        handler1_->handleMessage(topic, partition, offset, key, payload);
    }
    if (handler2_) {
        handler2_->handleMessage(topic, partition, offset, key, payload);
    }
}

class ConsumerEventCb : public RdKafka::EventCb {
public:
    void event_cb(RdKafka::Event& event) override {
        switch (event.type()) {
            case RdKafka::Event::EVENT_ERROR:
                std::cerr << "Kafka consumer error: " << RdKafka::err2str(event.err())
                          << ": " << event.str() << std::endl;
                break;
            case RdKafka::Event::EVENT_STATS:
                // 统计信息，可以用于监控
                break;
            case RdKafka::Event::EVENT_LOG:
                // 日志信息
                std::cout << "Kafka log: " << event.str() << std::endl;
                break;
            case RdKafka::Event::EVENT_THROTTLE:
                std::cout << "Kafka throttle: " << event.str() << std::endl;
                break;
            default:
                break;
        }
    }
};

class ConsumerRebalanceCb : public RdKafka::RebalanceCb {
public:
    void rebalance_cb(RdKafka::KafkaConsumer* consumer,
                     RdKafka::ErrorCode err,
                     std::vector<RdKafka::TopicPartition*>& partitions) override {
        std::cout << "Rebalance event: " << RdKafka::err2str(err) << std::endl;

        if (err == RdKafka::ERR__ASSIGN_PARTITIONS) {
            // 分区分配
            std::cout << "Assigned partitions: ";
            for (auto& partition : partitions) {
                std::cout << partition->topic() << "[" << partition->partition() << "] ";
            }
            std::cout << std::endl;

            consumer->assign(partitions);
        } else if (err == RdKafka::ERR__REVOKE_PARTITIONS) {
            // 分区撤销
            std::cout << "Revoked partitions: ";
            for (auto& partition : partitions) {
                std::cout << partition->topic() << "[" << partition->partition() << "] ";
            }
            std::cout << std::endl;

            consumer->unassign();
        }
    }
};

LibrdKafkaConsumer::LibrdKafkaConsumer(const KafkaConsumerConfig& config)
    : config_(config)
    , consumer_handle_(nullptr)
    , running_(false)
    , initialized_(false) {
    if (!initializeConsumer()) {
        throw std::runtime_error("Failed to initialize Kafka consumer");
    }
}

LibrdKafkaConsumer::~LibrdKafkaConsumer() {
    stop();

    if (consumer_handle_) {
        delete static_cast<RdKafka::KafkaConsumer*>(consumer_handle_);
        consumer_handle_ = nullptr;
    }

    // 清理 librdkafka 资源
    RdKafka::wait_destroyed(5000);
}

bool LibrdKafkaConsumer::initializeConsumer() {
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

        if (!config_.group_id.empty()) {
            if (conf->set("group.id", config_.group_id, errstr) != RdKafka::Conf::CONF_OK) {
                std::cerr << "Failed to set group.id: " << errstr << std::endl;
                delete conf;
                return false;
            }
        }

        if (!config_.client_id.empty()) {
            if (conf->set("client.id", config_.client_id, errstr) != RdKafka::Conf::CONF_OK) {
                std::cerr << "Failed to set client.id: " << errstr << std::endl;
                delete conf;
                return false;
            }
        }

        // 自动提交配置
        std::string enable_auto_commit_str = config_.enable_auto_commit ? "true" : "false";
        if (conf->set("enable.auto.commit", enable_auto_commit_str, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set enable.auto.commit: " << errstr << std::endl;
            delete conf;
            return false;
        }

        if (conf->set("auto.commit.interval.ms", std::to_string(config_.auto_commit_interval_ms), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set auto.commit.interval.ms: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 会话和超时配置
        if (conf->set("session.timeout.ms", std::to_string(config_.session_timeout_ms), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set session.timeout.ms: " << errstr << std::endl;
            delete conf;
            return false;
        }

        if (conf->set("max.poll.interval.ms", std::to_string(config_.max_poll_interval_ms), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set max.poll.interval.ms: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 自动偏移重置
        if (conf->set("auto.offset.reset", config_.auto_offset_reset, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set auto.offset.reset: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // 设置回调
        ConsumerEventCb* event_cb = new ConsumerEventCb();
        if (conf->set("event_cb", event_cb, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set event callback: " << errstr << std::endl;
            delete event_cb;
            delete conf;
            return false;
        }

        ConsumerRebalanceCb* rebalance_cb = new ConsumerRebalanceCb();
        if (conf->set("rebalance_cb", rebalance_cb, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Failed to set rebalance callback: " << errstr << std::endl;
            delete rebalance_cb;
            delete event_cb;
            delete conf;
            return false;
        }

        // 创建消费者
        RdKafka::KafkaConsumer* consumer = RdKafka::KafkaConsumer::create(conf, errstr);
        if (!consumer) {
            std::cerr << "Failed to create consumer: " << errstr << std::endl;
            delete rebalance_cb;
            delete event_cb;
            delete conf;
            return false;
        }

        // 保存句柄
        consumer_handle_ = consumer;
        initialized_ = true;

        std::cout << "Kafka consumer initialized successfully" << std::endl;
        std::cout << "Bootstrap servers: " << config_.get_bootstrap_servers_string() << std::endl;
        std::cout << "Topic: " << config_.topic << std::endl;
        std::cout << "Group ID: " << config_.group_id << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during consumer initialization: " << e.what() << std::endl;
        return false;
    }
}

bool LibrdKafkaConsumer::start() {
    if (!initialized_ || !consumer_handle_) {
        std::cerr << "Consumer not initialized" << std::endl;
        return false;
    }

    if (running_) {
        std::cout << "Consumer is already running" << std::endl;
        return true;
    }

    try {
        RdKafka::KafkaConsumer* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_handle_);

        // 订阅主题
        std::vector<std::string> topics = {config_.topic};
        RdKafka::ErrorCode err = consumer->subscribe(topics);
        if (err != RdKafka::ERR_NO_ERROR) {
            std::cerr << "Failed to subscribe to topic " << config_.topic
                      << ": " << RdKafka::err2str(err) << std::endl;
            return false;
        }

        // 启动消费者线程
        running_ = true;
        consumer_thread_ = std::thread(&LibrdKafkaConsumer::consumerThread, this);

        std::cout << "Kafka consumer started, subscribed to topic: " << config_.topic << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during consumer start: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void LibrdKafkaConsumer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 等待消费者线程结束
    if (consumer_thread_.joinable()) {
        consumer_thread_.join();
    }

    if (consumer_handle_) {
        RdKafka::KafkaConsumer* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_handle_);
        consumer->close();
    }

    std::cout << "Kafka consumer stopped" << std::endl;
}

std::string LibrdKafkaConsumer::getStatus() const {
    if (!initialized_) {
        return "Not initialized";
    }

    if (!running_) {
        return "Stopped";
    }

    return "Running";
}

void LibrdKafkaConsumer::setMessageHandler(std::shared_ptr<IKafkaMessageHandler> handler) {
    message_handler_ = handler;
}

void LibrdKafkaConsumer::consumerThread() {
    RdKafka::KafkaConsumer* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_handle_);

    while (running_) {
        try {
            // 轮询消息
            RdKafka::Message* message = consumer->consume(1000); // 1秒超时

            if (message) {
                processMessage(message);
                delete message;
            }

        } catch (const std::exception& e) {
            std::cerr << "Exception in consumer thread: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void LibrdKafkaConsumer::processMessage(RdKafka::Message* message) {
    if (!message_handler_) {
        return;
    }

    try {
        // 检查消息类型
        switch (message->err()) {
            case RdKafka::ERR__TIMED_OUT:
                // 超时，忽略
                return;

            case RdKafka::ERR_NO_ERROR:
                // 正常消息
                {
                    std::string topic = message->topic_name();
                    int32_t partition = message->partition();
                    int64_t offset = message->offset();

                    std::string key;
                    if (message->key()) {
                        key.assign(static_cast<const char*>(message->key_pointer()),
                                 message->key_len());
                    }

                    std::string payload;
                    if (message->payload()) {
                        payload.assign(static_cast<const char*>(message->payload()),
                                     message->len());
                    }

                    message_handler_->handleMessage(topic, partition, offset, key, payload);
                }
                break;

            default:
                // 错误消息
                std::cerr << "Consumer error: " << message->errstr() << std::endl;
                break;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception processing message: " << e.what() << std::endl;
    }
}

} // namespace data_processor
