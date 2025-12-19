#pragma once

#include "data_point.hpp"
#include "client.hpp"
#include <iostream>
#include <memory>

namespace opcuaclient {

/**
 * @brief 控制台数据处理器
 * 用于将数据点输出到控制台的简单实现
 */
class ConsoleDataHandler : public IDataPointHandler {
public:
    /**
     * @brief 处理接收到的数据点
     * @param data_point 数据点
     */
    void handleDataPoint(const DataPoint& data_point) override;

    /**
     * @brief 设置是否显示详细信息
     * @param verbose 是否详细输出
     */
    void setVerbose(bool verbose) { verbose_ = verbose; }

private:
    bool verbose_ = true;  ///< 是否详细输出
};

/**
 * @brief 数据采集器
 * 封装OPC UA客户端和数据处理器的完整采集流程
 */
class DataCollector {
public:
    /**
     * @brief 构造函数
     * @param config OPC UA配置
     */
    explicit DataCollector(const OpcUaConfig& config);

    /**
     * @brief 析构函数
     */
    ~DataCollector();

    /**
     * @brief 启动数据采集
     * @return 启动是否成功
     */
    bool start();

    /**
     * @brief 停止数据采集
     */
    void stop();

    /**
     * @brief 获取客户端状态
     */
    ClientState getClientState() const;

    /**
     * @brief 设置数据处理器
     * @param handler 数据处理器
     */
    void setDataHandler(std::shared_ptr<IDataPointHandler> handler);

private:
    const OpcUaConfig& config_;                          ///< 配置引用
    std::unique_ptr<OpcUaClient> client_;                ///< OPC UA客户端
    std::shared_ptr<IDataPointHandler> data_handler_;    ///< 数据处理器
};

} // namespace opcua
