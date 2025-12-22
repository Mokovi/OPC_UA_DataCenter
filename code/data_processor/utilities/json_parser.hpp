#pragma once

#include "../redis_client/redis_client.hpp"
#include <string>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace data_processor {

/**
 * @brief JSON 消息解析器
 */
class JsonMessageParser {
public:
    /**
     * @brief 解析 Kafka 消息中的数据点
     * @param json_payload JSON 格式的消息内容
     * @return 解析后的数据点，如果解析失败返回空
     */
    static std::optional<DataPoint> parseDataPoint(const std::string& json_payload);

private:
    /**
     * @brief 从 JSON 值中提取字符串
     * @param value JSON 值
     * @param default_value 默认值
     * @return 提取的字符串
     */
    static std::string extractString(const rapidjson::Value& value,
                                   const std::string& default_value = "");

    /**
     * @brief 从 JSON 值中提取整数
     * @param value JSON 值
     * @param default_value 默认值
     * @return 提取的整数
     */
    static int64_t extractInt64(const rapidjson::Value& value,
                               int64_t default_value = 0);

    /**
     * @brief 从 JSON 值中提取整数
     * @param value JSON 值
     * @param default_value 默认值
     * @return 提取的整数
     */
    static int extractInt(const rapidjson::Value& value,
                         int default_value = 0);
};

} // namespace data_processor
