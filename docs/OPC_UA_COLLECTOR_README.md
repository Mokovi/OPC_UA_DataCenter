# OPC UA 数据采集器使用指南

## 概述

OPC UA 数据采集器是分布式高可用数据采集与分析系统的核心组件之一，用于从 OPC UA 服务器（如 KepserverEx）采集工业设备数据。

## 功能特性

- 基于 open62541pp 实现的 OPC UA 客户端
- 支持配置文件驱动的节点订阅
- 实时数据采集和状态监控
- 模块化设计，便于扩展和维护

## 构建要求

- C++17 编译器
- CMake 3.16+
- open62541pp 库

## 构建步骤

```bash
# 创建构建目录
mkdir build && cd build

# 配置构建
cmake ..

# 编译
make -j$(nproc)

# 生成的可执行文件
ls -la opcua_collector
```

## 配置说明

### 主配置文件 (config)

```ini
# OPC UA 服务器连接配置
OPC_UA_URL = opc.tcp://192.168.10.17:49320
OPC_UA_SecurityMode = None

# 连接参数
ConnectionTimeout = 5000
SessionTimeout = 30000
SubscriptionInterval = 1000
```

### 节点列表文件 (nodes.txt)

```
# OPC UA 节点列表
# 格式：NamespaceIndex.NodeId
Sim.Device1.Test1
Sim.Device1.Test2
Sim.Device1.Test3
```

## 运行方式

```bash
# 使用默认配置文件
./opcua_collector

# 指定自定义配置文件
./opcua_collector /path/to/config /path/to/nodes.txt

# 查看帮助信息
./opcua_collector --help
```

## 输出示例

程序运行时会显示类似以下的输出：

```
OPC UA Data Collection System
Loading configuration from: config
Loading nodes from: nodes.txt

Configuration loaded successfully:
- Server URL: opc.tcp://192.168.10.17:49320
- Security Mode: None
- Nodes to monitor: 3

OPC UA client started, connecting to: opc.tcp://192.168.10.17:49320
OPC UA session activated
Created subscription for node: Sim.Device1.Test1
Created subscription for node: Sim.Device1.Test2
Created subscription for node: Sim.Device1.Test3
[2025-12-19 15:15:22] DATA - Node: Sim.Device1.Test1, Value: 123, Quality: Good
[2025-12-19 15:15:22] DATA - Node: Sim.Device1.Test2, Value: 456, Quality: Good
```

## 故障排除

### 连接失败

1. 检查 OPC UA 服务器是否运行
2. 验证网络连通性：`ping <server_ip>`
3. 检查防火墙设置
4. 确认服务器 URL 和端口正确

### 数据采集异常

1. 验证节点ID格式是否正确
2. 检查 OPC UA 服务器中的节点是否存在
3. 查看服务器日志了解访问权限问题
4. 确认命名空间索引（通常为2）

### 程序崩溃

1. 检查 open62541pp 库版本兼容性
2. 验证配置文件格式
3. 查看系统资源使用情况
4. 检查内存和网络连接

## 开发说明

### 代码结构

```
src/
├── main.cpp                 # 程序入口
└── opcua/
    ├── config.hpp/cpp       # 配置加载
    ├── data_point.hpp/cpp   # 数据点模型
    ├── client.hpp/cpp       # OPC UA 客户端
    └── data_collector.hpp/cpp # 数据采集器
```

### 扩展功能

- 添加更多数据处理器（如写入 Kafka、Redis 等）
- 实现更复杂的数据质量判断
- 添加监控和告警功能
- 支持更多 OPC UA 特性（如历史数据读取、安全连接等）

## 性能考虑

- 默认采样间隔：1000ms，可根据需要调整
- 支持多节点并发订阅
- 内存使用与订阅节点数量成正比
- 网络延迟影响数据实时性
