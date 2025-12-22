#include "json_parser.hpp"
#include <iostream>

namespace data_processor {

std::optional<DataPoint> JsonMessageParser::parseDataPoint(const std::string& json_payload) {
    rapidjson::Document doc;

    // 解析 JSON
    if (doc.Parse(json_payload.c_str()).HasParseError()) {
        std::cerr << "JSON parse error: " << rapidjson::GetParseError_En(doc.GetParseError())
                  << " at offset " << doc.GetErrorOffset() << std::endl;
        return std::nullopt;
    }

    // 检查是否为对象
    if (!doc.IsObject()) {
        std::cerr << "JSON root is not an object" << std::endl;
        return std::nullopt;
    }

    DataPoint data_point;

    // 提取必需字段
    if (doc.HasMember("source_id") && doc["source_id"].IsString()) {
        data_point.source_id = doc["source_id"].GetString();
    } else {
        std::cerr << "Missing or invalid 'source_id' field" << std::endl;
        return std::nullopt;
    }

    if (doc.HasMember("node_id") && doc["node_id"].IsString()) {
        data_point.node_id = doc["node_id"].GetString();
    } else {
        std::cerr << "Missing or invalid 'node_id' field" << std::endl;
        return std::nullopt;
    }

    // 提取可选字段
    data_point.value = extractString(doc["value"], "");
    data_point.timestamp = extractInt64(doc["ingest_timestamp"], 0); // 使用 ingest_timestamp 作为主时间戳
    data_point.quality = extractInt(doc["quality"], 0);

    return data_point;
}

std::string JsonMessageParser::extractString(const rapidjson::Value& value,
                                           const std::string& default_value) {
    if (value.IsString()) {
        return value.GetString();
    }
    return default_value;
}

int64_t JsonMessageParser::extractInt64(const rapidjson::Value& value,
                                       int64_t default_value) {
    if (value.IsInt64()) {
        return value.GetInt64();
    } else if (value.IsInt()) {
        return static_cast<int64_t>(value.GetInt());
    }
    return default_value;
}

int JsonMessageParser::extractInt(const rapidjson::Value& value,
                                 int default_value) {
    if (value.IsInt()) {
        return value.GetInt();
    } else if (value.IsInt64()) {
        // 检查是否在 int 范围内
        int64_t val = value.GetInt64();
        if (val >= std::numeric_limits<int>::min() && val <= std::numeric_limits<int>::max()) {
            return static_cast<int>(val);
        }
    }
    return default_value;
}

} // namespace data_processor
