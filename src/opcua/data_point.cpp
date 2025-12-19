#include "data_point.hpp"
#include <sstream>
#include <iomanip>

namespace opcuaclient {

DataPoint::DataPoint(std::string source, std::string node, std::string val_str,
                     DataQuality qual, std::chrono::system_clock::time_point device_time)
    : source_id(std::move(source))
    , node_id(std::move(node))
    , value(std::move(val_str))
    , device_timestamp(device_time)
    , ingest_timestamp(std::chrono::system_clock::now())
    , quality(qual) {
}

std::string DataPoint::valueAsString() const {
    return value;
}

} // namespace opcua
