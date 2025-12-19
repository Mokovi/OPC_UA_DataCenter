#include "client.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace opcuaclient {

OpcUaClient::OpcUaClient(const OpcUaConfig& config, std::shared_ptr<IDataPointHandler> data_handler)
    : config_(config)
    , data_handler_(data_handler)
    , state_(ClientState::Disconnected)
    , running_(false) {
    client_ = std::make_unique<opcua::Client>();
}

OpcUaClient::~OpcUaClient() {
    stop();
}

bool OpcUaClient::start() {
    if (running_) {
        std::cout << "Client is already running" << std::endl;
        return true;
    }

    running_ = true;
    updateState(ClientState::Connecting);

    try {
        // 设置会话回调
        setupSessionCallbacks();

        // 启动工作线程
        worker_thread_ = std::thread(&OpcUaClient::workerThread, this);

        std::cout << "OPC UA client started, connecting to: " << config_.server_url << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to start OPC UA client: " << e.what() << std::endl;
        running_ = false;
        updateState(ClientState::Error);
        return false;
    }
}

void OpcUaClient::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    updateState(ClientState::Disconnected);

    // 等待工作线程结束
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // 清理资源
    deleteSubscriptions();
    disconnect();

    std::cout << "OPC UA client stopped" << std::endl;
}

ClientState OpcUaClient::getState() const {
    return state_.load();
}

std::string OpcUaClient::getStateString() const {
    switch (getState()) {
        case ClientState::Disconnected: return "Disconnected";
        case ClientState::Connecting: return "Connecting";
        case ClientState::Connected: return "Connected";
        case ClientState::SessionActive: return "SessionActive";
        case ClientState::Error: return "Error";
        default: return "Unknown";
    }
}

void OpcUaClient::workerThread() {
    while (running_) {
        try {
            if (getState() == ClientState::Connecting || getState() == ClientState::Disconnected || getState() == ClientState::Error) {
                if (connect()) {
                    updateState(ClientState::Connected);
                    // 等待会话激活
                    std::unique_lock<std::mutex> lock(state_mutex_);
                    state_cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
                        return getState() == ClientState::SessionActive || !running_;
                    });
                }
            }

            if (getState() == ClientState::Connected || getState() == ClientState::SessionActive) {
                // 处理客户端事件
                client_->runIterate(100);
            } else {
                // 连接失败，等待重试
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }

        } catch (const opcua::BadStatus& e) {
            handleConnectionError(std::string("OPC UA status error: ") + e.what());
            std::this_thread::sleep_for(std::chrono::seconds(3));
        } catch (const std::exception& e) {
            handleConnectionError(std::string("Unexpected error: ") + e.what());
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
}

bool OpcUaClient::connect() {
    try {
        client_->connect(config_.server_url);
        std::cout << "Successfully connected to OPC UA server" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect to OPC UA server: " << e.what() << std::endl;
        return false;
    }
}

void OpcUaClient::disconnect() {
    try {
        if (client_) {
            client_->disconnect();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during disconnect: " << e.what() << std::endl;
    }
}

void OpcUaClient::setupSessionCallbacks() {
    client_->onSessionActivated([this]() {
        std::cout << "OPC UA session activated" << std::endl;
        updateState(ClientState::SessionActive);

        // 创建订阅和监控项
        if (!createSubscriptions()) {
            std::cerr << "Failed to create subscriptions" << std::endl;
            updateState(ClientState::Error);
        }
    });

    client_->onSessionClosed([this]() {
        std::cout << "OPC UA session closed" << std::endl;
        updateState(ClientState::Connected);
        deleteSubscriptions();
    });

    client_->onConnected([]() {
        std::cout << "OPC UA client connected" << std::endl;
    });

    client_->onDisconnected([]() {
        std::cout << "OPC UA client disconnected" << std::endl;
    });
}

bool OpcUaClient::createSubscriptions() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    try {
        // 为每个启用的节点创建订阅
        for (const auto& node_config : config_.nodes) {
            if (!node_config.enabled) {
                continue;
            }

            // 创建订阅
            opcua::Subscription<opcua::Client> subscription(*client_);

            // 设置订阅参数
            opcua::SubscriptionParameters params;
            params.publishingInterval = config_.subscription_interval_ms;
            subscription.setSubscriptionParameters(params);
            subscription.setPublishingMode(true);

            // 创建监控项
            auto monitored_item = subscription.subscribeDataChange(
                opcua::NodeId(2, node_config.node_id),  // 假设命名空间索引为2
                opcua::AttributeId::Value,
                [this, node_id = node_config.node_id](opcua::IntegerId, opcua::IntegerId,
                                                     const opcua::DataValue& data_value) {
                    this->handleDataChange(node_id, data_value);
                }
            );

            // 设置监控参数
            opcua::MonitoringParametersEx monitoring_params;
            monitoring_params.samplingInterval = node_config.sampling_interval_ms;
            monitored_item.setMonitoringParameters(monitoring_params);
            monitored_item.setMonitoringMode(opcua::MonitoringMode::Reporting);

            subscriptions_.push_back(std::move(subscription));

            std::cout << "Created subscription for node: " << node_config.node_id << std::endl;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to create subscriptions: " << e.what() << std::endl;
        return false;
    }
}

void OpcUaClient::deleteSubscriptions() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.clear();
}

void OpcUaClient::handleDataChange(const std::string& node_id, const opcua::DataValue& data_value) {
    try {
        // 简化数据处理：直接转换为字符串表示
        std::string value_str;
        if (data_value.value().isScalar()) {
            // 对于标量值，尝试转换为字符串
            auto variant = data_value.value();
            if (variant.isType<bool>()) {
                value_str = variant.scalar<bool>() ? "true" : "false";
            } else if (variant.isType<int32_t>()) {
                value_str = std::to_string(variant.scalar<int32_t>());
            } else if (variant.isType<uint32_t>()) {
                value_str = std::to_string(variant.scalar<uint32_t>());
            } else if (variant.isType<float>()) {
                value_str = std::to_string(variant.scalar<float>());
            } else if (variant.isType<double>()) {
                value_str = std::to_string(variant.scalar<double>());
            } else {
                value_str = "Unsupported scalar type";
            }
        } else {
            value_str = "Non-scalar value";
        }

        // 创建设据点
        DataQuality quality = DataQuality::Good;
        // 简化质量判断，暂时都设为Good
        // TODO: 根据实际需求实现质量判断

        DataPoint data_point(config_.server_url, node_id, static_cast<std::string>(value_str), quality);

        // 调用数据处理器
        if (data_handler_) {
            data_handler_->handleDataPoint(data_point);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error handling data change for node " << node_id << ": " << e.what() << std::endl;
    }
}

void OpcUaClient::handleConnectionError(const std::string& error_message) {
    std::cerr << "Connection error: " << error_message << std::endl;
    updateState(ClientState::Error);
    deleteSubscriptions();
}

void OpcUaClient::updateState(ClientState new_state) {
    state_.store(new_state);
    state_cv_.notify_all();
}

} // namespace opcua
