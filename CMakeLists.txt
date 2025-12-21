cmake_minimum_required(VERSION 3.16)
project(opcua-data-collector VERSION 0.1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找依赖包
find_package(open62541pp REQUIRED)
find_package(Threads REQUIRED)

# 查找 librdkafka
find_library(RDKAFKA_LIBRARY rdkafka++)
find_path(RDKAFKA_INCLUDE_DIR librdkafka/rdkafkacpp.h)

if(NOT RDKAFKA_LIBRARY OR NOT RDKAFKA_INCLUDE_DIR)
    message(FATAL_ERROR "librdkafka not found. Please install librdkafka development package.")
endif()

# 包含目录
include_directories(code)

# 源文件
set(OPCUA_SOURCES
    code/data_collector/main.cpp
    code/data_collector/opcua_client/config.cpp
    code/data_collector/opcua_client/data_point.cpp
    code/data_collector/opcua_client/client.cpp
    code/data_collector/opcua_client/data_collector.cpp
    code/data_collector/kafka_producer/kafka_producer.cpp
)

# 创建可执行文件
add_executable(opcua_collector ${OPCUA_SOURCES})

# 链接库
target_link_libraries(opcua_collector
    PRIVATE
        open62541pp::open62541pp
        Threads::Threads
        ${RDKAFKA_LIBRARY}
)

# 包含目录
target_include_directories(opcua_collector PRIVATE ${RDKAFKA_INCLUDE_DIR})

# 编译选项
target_compile_options(opcua_collector PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -ggdb
)

# 安装目标（可选）
install(TARGETS opcua_collector
    RUNTIME DESTINATION bin
)
