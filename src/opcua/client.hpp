#pragma once

#include "config.hpp"
#include "data_point.hpp"
#include <open62541pp/client.hpp>
#include <open62541pp/subscription.hpp>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace opcuaclient {

/**
 * @brief OPC UA 客户端状态
 */
enum class ClientState {
    Disconnected = 0,   ///< 未连接
    Connecting = 1,     ///< 正在连接
    Connected = 2,      ///< 已连接
    SessionActive = 3,  ///< 会话激活
    Error = 4           ///< 错误状态
};

/**
 * @brief OPC UA 采集客户端
 */
class OpcUaClient {
public:
    /**
     * @brief 构造函数
     * @param config 客户端配置
     * @param data_handler 数据处理器
     */
    OpcUaClient(const OpcUaConfig& config, std::shared_ptr<IDataPointHandler> data_handler);

    /**
     * @brief 析构函数
     */
    ~OpcUaClient();

    /**
     * @brief 启动客户端
     * @return 启动是否成功
     */
    bool start();

    /**
     * @brief 停止客户端
     */
    void stop();

    /**
     * @brief 获取当前状态
     */
    ClientState getState() const;

    /**
     * @brief 获取状态描述
     */
    std::string getStateString() const;

private:
    /**
     * @brief 客户端工作线程
     */
    void workerThread();

    /**
     * @brief 建立连接
     */
    bool connect();

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 设置会话回调
     */
    void setupSessionCallbacks();

    /**
     * @brief 创建订阅和监控项
     */
    bool createSubscriptions();

    /**
     * @brief 删除所有订阅
     */
    void deleteSubscriptions();

    /**
     * @brief 处理连接错误
     */
    void handleConnectionError(const std::string& error_message);

    /**
     * @brief 处理数据变化
     */
    void handleDataChange(const std::string& node_id, const opcua::DataValue& data_value);

    /**
     * @brief 更新客户端状态
     */
    void updateState(ClientState new_state);

    const OpcUaConfig& config_;                          ///< 客户端配置
    std::shared_ptr<IDataPointHandler> data_handler_;    ///< 数据处理器

    std::unique_ptr<opcua::Client> client_;             ///< OPC UA 客户端实例
    std::atomic<ClientState> state_;                     ///< 当前状态
    std::thread worker_thread_;                          ///< 工作线程
    std::atomic<bool> running_;                          ///< 运行标志
    mutable std::mutex state_mutex_;                     ///< 状态保护互斥锁
    std::condition_variable state_cv_;                   ///< 状态条件变量

    // 订阅管理
    std::vector<opcua::Subscription<opcua::Client>> subscriptions_;    ///< 活跃的订阅列表
    std::mutex subscriptions_mutex_;                     ///< 订阅保护互斥锁
};

} // namespace opcua
