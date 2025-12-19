#pragma once

#include <string>
#include <chrono>
#include <variant>
#include <optional>

namespace opcuaclient {

/**
 * @brief 数据质量枚举
 */
enum class DataQuality {
    Good = 0,           ///< 数据质量良好
    Uncertain = 1,      ///< 数据不确定
    Bad = 2             ///< 数据质量差
};

/**
 * @brief 数据点值类型（简化版，使用字符串表示）
 */
using DataPointValue = std::string;

/**
 * @brief OPC UA 数据点
 */
struct DataPoint {
    std::string source_id;              ///< 数据源标识 (如服务器URL)
    std::string node_id;                ///< OPC UA 节点ID
    DataPointValue value;               ///< 数据值
    std::chrono::system_clock::time_point device_timestamp;  ///< 设备时间戳
    std::chrono::system_clock::time_point ingest_timestamp;  ///< 采集时间戳
    DataQuality quality;                ///< 数据质量
    std::optional<std::string> error_message;  ///< 错误信息（如果有）

    /**
     * @brief 创建数据点
     * @param source 数据源标识
     * @param node 节点ID
     * @param val_str 字符串表示的值
     * @param quality 数据质量
     * @param device_time 设备时间戳
     */
    DataPoint(std::string source, std::string node, std::string val_str,
              DataQuality qual = DataQuality::Good,
              std::chrono::system_clock::time_point device_time = std::chrono::system_clock::now());

    /**
     * @brief 获取数据值的字符串表示
     */
    std::string valueAsString() const;

    /**
     * @brief 检查数据质量是否良好
     */
    bool isGood() const { return quality == DataQuality::Good; }

    /**
     * @brief 检查是否有错误
     */
    bool hasError() const { return error_message.has_value(); }
};

/**
 * @brief 数据点处理器接口
 */
class IDataPointHandler {
public:
    virtual ~IDataPointHandler() = default;

    /**
     * @brief 处理接收到的数据点
     * @param data_point 数据点
     */
    virtual void handleDataPoint(const DataPoint& data_point) = 0;
};

} // namespace opcua
