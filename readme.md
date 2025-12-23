# 分布式数据采集与处理系统

## 📋 项目简介

本项目是一个基于OPC UA协议的分布式数据采集与处理原型系统。系统通过OPC UA客户端从工业设备采集实时数据，经过Kafka消息队列传递给数据处理器，最终存储到Redis缓存中。

这是一个原型验证项目，主要用于验证OPC UA数据采集、消息队列传输和实时数据存储的技术方案，为后续的工业物联网数据平台建设提供技术参考。

## 🏗️ 系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     数据处理层（Processing）                 │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Kafka消息队列（解耦层）                  │    │
│  └────────────────┬─────────────────────────────────────┘    │
│          ┌────────▼────────┐ ┌────────────────────────┐ │
│          │   数据处理器     │ │        Redis缓存       │ │
│          │ (Kafka消费者)   │ │    (实时数据存储)      │ │
│          └─────────────────┘ └────────────────────────┘ │
└────────────────────┬───────────────────────────────────────┘
                     │
┌────────────────────▼───────────────────────────────────────┐
│               数据采集层（Ingestion）                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              OPC UA 数据采集器                      │    │
│  │    ┌─────────────────────────────────────────────┐  │    │
│  │    │           open62541pp Client                │  │    │
│  │    │  ┌──────────────┐ ┌─────────────────────┐   │  │    │
│  │    │  │   连接管理    │ │     Kafka生产者      │   │  │    │
│  │    │  └──────────────┘ └─────────────────────┘   │  │    │
│  │    └─────────────────────────────────────────────┘  │    │
│  └─────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────┘
                          │ OPC UA
┌─────────────────────────▼────────────────────────────────────┐
│                 现场设备层（Field）                          │
│  ┌─────────────────────────────────────────────────────┐     │
│  │             KepserverEx（OPC UA Gateway）              │   │
│  └──────┬────────────────────┬────────────────────┬─────┘   │
│         │                    │                    │         │
│   ┌─────▼────┐       ┌──────▼──────┐     ┌───────▼────┐   │
│   │  PLC设备  │       │   传感器    │     │   仪表设备  │   │
│   └──────────┘       └─────────────┘     └────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 技术栈

- **编程语言**: C++17
- **构建工具**: CMake 3.16+
- **OPC UA**: open62541pp (C++包装库)
- **消息队列**: Apache Kafka (librdkafka)
- **缓存存储**: Redis 6.x (hiredis异步客户端)
- **JSON解析**: rapidjson

## 🎯 核心功能模块

### 1. OPC UA 数据采集器

**功能特性**:
- 基于open62541pp库实现OPC UA客户端
- 支持多节点数据订阅和实时采集
- 自动重连和错误恢复机制
- 配置文件驱动的节点管理

### 2. Kafka 消息传输

**功能特性**:
- 基于librdkafka的高性能消息生产者
- 异步消息发送和批量处理
- 支持消息确认和重试机制
- JSON格式的数据序列化

### 3. 数据处理与存储

**功能特性**:
- 基于librdkafka的消费者实现
- 实时JSON消息解析
- 异步Redis数据存储
- 控制台数据输出和调试支持

## 🚀 快速开始

### 环境要求

- **操作系统**: Linux/macOS
- **编译器**: GCC 7+ 或 Clang 5+
- **依赖库**:
  - open62541pp
  - librdkafka
  - hiredis
  - rapidjson

### 依赖安装 (Ubuntu/Debian)

```bash
# 安装系统依赖
sudo apt-get update
sudo apt-get install -y build-essential cmake

# 安装项目依赖
sudo apt-get install -y libopen62541pp-dev librdkafka-dev libhiredis-dev rapidjson-dev
```

### 构建项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置构建
cmake ..

# 编译项目
make -j$(nproc)
```

### 运行程序

```bash
# 启动数据采集器
./data_collector

# 启动数据处理器
./data_processor
```

## 📊 性能指标 (原型阶段)

| 指标 | 当前值 | 目标值 |
|------|--------|--------|
| 数据采集延迟 | < 200ms | < 100ms |
| 消息处理吞吐量 | ~500 msg/s | > 1000 msg/s |
| 内存占用 | < 50MB | < 100MB |
| CPU使用率 | < 10% | < 20% |

## 🔧 配置说明

### 主要配置文件 (config)

```ini
# OPC UA 服务器连接配置
OPC_UA_URL = opc.tcp://192.168.10.17:49320
OPC_UA_SecurityMode = None

# 连接参数
ConnectionTimeout = 5000
SessionTimeout = 30000
SubscriptionInterval = 1000

# Kafka 配置
KafkaBootstrapServers = localhost:9092
KafkaTopic = opcua-data
KafkaClientId = opcua-collector-01

# Redis 配置
RedisHost = localhost
RedisPort = 6379
RedisConnectionPoolSize = 10
```

### 节点列表文件 (nodes.txt)

```
# OPC UA 节点列表
# 格式：NamespaceIndex.NodeId
Sim.Device1.Test1
Sim.Device1.Test2
Sim.Device1.Test3
```

## 📁 项目结构

```
distributed-data-collection/
├── code/
│   ├── data_collector/        # 数据采集模块
│   │   ├── main.cpp
│   │   ├── opcua_client/      # OPC UA客户端子模块
│   │   │   ├── client.cpp/hpp
│   │   │   ├── config.cpp/hpp
│   │   │   ├── data_point.cpp/hpp
│   │   │   └── data_collector.cpp/hpp
│   │   └── kafka_producer/    # Kafka生产者子模块
│   │       ├── kafka_producer.cpp/hpp
│   ├── data_processor/        # 数据处理模块
│       ├── main.cpp
│       ├── utilities/         # 工具模块
│       │   ├── config.cpp/hpp # 配置管理
│       │   └── json_parser.cpp/hpp # JSON解析
│       ├── kafka_consumer/    # Kafka消费者子模块
│       │   ├── kafka_consumer.cpp/hpp
│       └── redis_client/      # Redis客户端子模块
│           ├── redis_client.cpp/hpp
├── config/                    # 配置文件
├── docs/                      # 项目文档
│   ├── architecture.md
│   ├── design/
│   │   └── high_level_design.md
│   ├── planning/
│   │   └── implementation_plan.md
│   ├── project_progress.md
│   └── snapshots/
└── third_party/               # 第三方依赖
```

## 🔍 故障排查

### 常见问题

1. **OPC UA连接失败**
   ```bash
   # 检查KepserverEx服务状态
   # 确认OPC UA服务器地址和端口是否正确
   # 检查防火墙设置
   ```

2. **Kafka连接失败**
   ```bash
   # 启动Kafka服务
   # 检查Kafka broker地址和端口
   # 验证topic是否存在
   ```

3. **Redis连接失败**
   ```bash
   # 启动Redis服务
   # 检查Redis服务器地址和端口
   # 确认连接池配置
   ```

## 🤝 开发指南

### 代码规范

- 使用C++17标准
- 遵循Google C++ Style Guide
- 所有代码需要通过编译器警告检查
- 新功能需要添加单元测试

### 提交规范

```
feat: 新功能
fix: 修复bug
docs: 文档更新
style: 代码格式调整
refactor: 代码重构
test: 测试相关
chore: 构建过程或工具配置更新
```

## 📄 许可证

本项目采用MIT许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

---

**⭐️ 这是一个原型验证项目，用于技术方案验证和学习目的。如需生产环境使用，请根据实际需求进行适当的修改和优化。**