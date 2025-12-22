# OPC UA 数据采集与处理系统使用指南

## 概述

OPC UA 数据采集与处理系统是分布式高可用数据采集与分析系统的核心组件，包括数据采集器和数据处理器两个主要部分：

- **数据采集器 (opcua_collector)**: 基于 open62541pp 实现的 OPC UA 客户端，从 OPC UA 服务器采集工业设备数据
- **数据处理器 (data_processor)**: 基于 librdkafka 和 hiredis 实现的高性能数据处理组件，支持 Kafka 消息消费和 Redis 数据存储

## 功能特性

### 数据采集器
- 基于 open62541pp 实现的 OPC UA 客户端
- 支持配置文件驱动的节点订阅
- 实时数据采集和状态监控
- 模块化设计，便于扩展和维护

### 数据处理器
- 基于 librdkafka 的高性能 Kafka 消费者
- 支持 JSON 消息解析 (使用 rapidjson)
- 基于 hiredis 的异步 Redis 客户端
- Redis Hash 数据存储，支持高速缓存和查询
- 复合消息处理器，同时支持控制台输出和 Redis 存储

## 构建要求

### 数据采集器
- C++17 编译器
- CMake 3.16+
- open62541pp 库

### 数据处理器
- C++17 编译器
- CMake 3.16+
- librdkafka 库 (librdkafka++ 和 librdkafka)
- hiredis 库
- rapidjson 库

#### 依赖包安装 (Ubuntu/Debian)
```bash
# 数据采集器依赖
sudo apt-get install libopen62541pp-dev

# 数据处理器依赖
sudo apt-get install librdkafka-dev hiredis-dev rapidjson-dev
```

## 构建步骤

```bash
# 创建构建目录
mkdir build && cd build

# 配置构建
cmake ..

# 编译
make -j$(nproc)

# 生成的可执行文件
ls -la opcua_collector data_processor
```

构建系统会自动检测依赖库，如果缺少可选依赖（如 hiredis、rapidjson），会生成相应的警告并禁用相关功能。

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
code/
├── data_collector/          # 数据采集模块 (opcua_collector)
│   ├── main.cpp             # 程序入口
│   ├── opcua_client/        # OPC UA 客户端子模块
│   │   ├── config.hpp/cpp   # 配置加载
│   │   ├── data_point.hpp/cpp # 数据点模型
│   │   ├── client.hpp/cpp   # OPC UA 客户端
│   │   └── data_collector.hpp/cpp # 数据采集器
│   └── kafka_producer/      # Kafka 生产者子模块
│       └── kafka_producer.hpp/cpp # Kafka 消息发送
└── data_processor/          # 数据处理模块 (data_processor)
    ├── main.cpp             # 程序入口
    ├── config.hpp/cpp       # 配置管理
    └── kafka_consumer/      # Kafka 消费者子模块
        └── kafka_consumer.hpp/cpp # Kafka 消息消费
```

## Kafka 集成说明

### 配置参数

```ini
# Kafka 配置
KafkaBootstrapServers = localhost:9092,localhost:9093
KafkaTopic = opcua-data
KafkaClientId = opcua-collector-01
KafkaAcks = 1
KafkaRetries = 3
KafkaBatchSize = 16384
KafkaLingerMs = 5
```

### 消息格式

采集到的数据点将以 JSON 格式发送到 Kafka：

```json
{
  "source_id": "opc.tcp://192.168.10.17:49320",
  "node_id": "Sim.Device1.Test1",
  "value": "123.45",
  "device_timestamp": 1734768000000,
  "ingest_timestamp": 1734768000500,
  "quality": 0
}
```

### 部署要求

- 安装 librdkafka 开发包：`sudo apt-get install librdkafka-dev`
- 确保 Kafka 服务器可访问
- 配置适当的主题和分区策略

## 数据处理器使用说明

### 概述

数据处理器 (`data_processor`) 是一个独立的可执行程序，用于从 Kafka 中消费数据并进行处理。目前主要功能是从 Kafka 消费消息并输出到控制台，为后续的数据清洗、存储和分析奠定基础。

### 编译和运行

```bash
# 编译
cd build && make data_processor

# 运行
./data_processor [config_file]

# 查看帮助
./data_processor --help
```

### 配置参数

数据处理器使用与采集器相同的配置文件，但重点关注 Kafka 消费者配置：

```ini
# Kafka 消费者配置
KafkaBootstrapServers = localhost:9092
KafkaTopic = opcua-data
KafkaGroupId = data-processor-group
KafkaClientId = data-processor-01
KafkaAutoCommit = true
KafkaAutoCommitInterval = 5000
KafkaSessionTimeout = 30000

# Redis 配置
RedisHost = localhost
RedisPort = 6379
RedisPassword =
RedisConnectionTimeout = 5000
RedisConnectionPoolSize = 10

# MySQL 配置 (预留)
MySQLHost = localhost
MySQLPort = 3306
MySQLUsername = root
MySQLPassword = password
MySQLDatabase = opcua_data
MySQLCharset = utf8mb4
MySQLConnectionTimeout = 30

# 数据处理器配置
EnableConsoleOutput = true
ProcessingThreads = 4
MaxBatchSize = 100
```

### 消息格式

消费的 Kafka 消息格式如下：

```json
{
  "source_id": "opc.tcp://192.168.10.17:49320",
  "node_id": "Sim.Device1.Test1",
  "value": "123.45",
  "device_timestamp": 1734768000000,
  "ingest_timestamp": 1734768000500,
  "quality": 0
}
```

### 输出示例

程序运行时会显示类似以下的输出：

```
Data Processing System
Loading configuration from: config

Configuration loaded successfully:
- Kafka servers: localhost:9092
- Kafka topic: opcua-data
- Kafka group: data-processor-group
- Redis: localhost:6379
- MySQL: localhost:3306/opcua_data

Kafka consumer initialized successfully
Bootstrap servers: localhost:9092
Topic: opcua-data
Group ID: data-processor-group
Kafka consumer started, subscribed to topic: opcua-data

[2025-12-21 15:42:01] KAFKA - Topic: opcua-data, Partition: 0, Offset: 123, Payload: {"source_id":"opc.tcp://192.168.10.17:49320","node_id":"Sim.Device1.Test1","value":"123.45","device_timestamp":1734768000000,"ingest_timestamp":1734768000500,"quality":0}
[2025-12-21 15:42:02] KAFKA - Topic: opcua-data, Partition: 0, Offset: 124, Payload: {"source_id":"opc.tcp://192.168.10.17:49320","node_id":"Sim.Device1.Test2","value":"true","device_timestamp":1734768001000,"ingest_timestamp":1734768001500,"quality":0}
```

### 数据处理器使用示例

启动数据处理器：

```bash
# 使用默认配置文件
./data_processor

# 指定配置文件
./data_processor /path/to/config
```

运行输出示例：

```
Data Processing System
Loading configuration from: config

Kafka consumer started, subscribed to topic: opcua-data
Redis client started successfully
Consumer Status: Connected | Redis Status: Connected | Redis Stats: 125/125 stored
```

## Redis 存储说明

### 概述

数据处理器支持将 Kafka 中消费到的数据实时存储到 Redis 中，实现高速数据缓存和查询功能。每个数据点以 Hash 结构存储在 Redis 中，便于快速访问和更新。

### Redis 数据结构

每个 OPC UA 数据点在 Redis 中存储为一个 Hash，键格式为：
```
DataPoint:{node_id}
```

Hash 字段包括：
- `value`: 数据值（字符串形式）
- `updated_at`: 最后更新时间戳（毫秒）
- `quality`: 数据质量

### 示例查询

```bash
# 查看数据点信息
HGETALL "DataPoint:Sim.Device1.Test1"

# 获取特定字段
HGET "DataPoint:Sim.Device1.Test1" value
HGET "DataPoint:Sim.Device1.Test1" updated_at
HGET "DataPoint:Sim.Device1.Test1" quality
```

### 性能特性

- **异步操作**: 非阻塞的数据存储操作
- **批量写入**: 支持批量数据点存储
- **自动清理**: 7天过期时间，防止无限增长
- **错误恢复**: 自动重连和故障恢复
- **高并发**: 基于 hiredis 的高性能异步客户端

### 扩展功能

- 添加更多数据处理器（如写入 Redis、数据库等）
- 实现更复杂的数据质量判断和过滤
- 添加监控和告警功能
- 支持更多 OPC UA 特性（如历史数据读取、安全连接等）
- 实现消息压缩和批量优化

## 性能考虑

- 默认采样间隔：1000ms，可根据需要调整
- 支持多节点并发订阅
- 内存使用与订阅节点数量成正比
- 网络延迟影响数据实时性
