cmake_minimum_required(VERSION 3.16)
project(opcua-data-collector VERSION 0.1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找依赖包
find_package(open62541pp REQUIRED)
find_package(Threads REQUIRED)

# 包含目录
include_directories(code)

# 源文件
set(OPCUA_SOURCES
    code/main.cpp
    code/opcua_client/config.cpp
    code/opcua_client/data_point.cpp
    code/opcua_client/client.cpp
    code/opcua_client/data_collector.cpp
)

# 创建可执行文件
add_executable(opcua_collector ${OPCUA_SOURCES})

# 链接库
target_link_libraries(opcua_collector
    PRIVATE
        open62541pp::open62541pp
        Threads::Threads
)

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
