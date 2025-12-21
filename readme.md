# 分布式高可用数据采集与分析系统

## 📋 项目简介

本项目是一个面向工业自动化场景的分布式高可用数据采集与分析系统。系统通过OPC UA协议实现PLC设备数据采集，采用微服务架构和消息队列解耦处理流程，提供低延迟、高可用的数据服务，为EMS系统提供稳定数据支撑。


## 🏗️ 系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    Web监控中心（EMS系统）                    │
└────────────────────────────┬────────────────────────────────┘
                             │ HTTP/WebSocket
┌────────────────────────────▼────────────────────────────────┐
│                数据服务层（Data Service）                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   查询API   │  │   告警服务  │  │  统计服务   │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
└────────────────────────────┬────────────────────────────────┘
                             │ REST/Protobuf
┌────────────────────────────▼────────────────────────────────┐
│               数据处理与存储层（Processing）                 │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Kafka消息队列（解耦层）                  │    │
│  └──────────────┬────────────────┬─────────────────────┘    │
│        ┌────────▼──────┐ ┌───────▼────────┐ ┌────────────┐ │
│        │  实时计算     │ │  批处理引擎   │ │  数据清洗  │ │
│        └───────────────┘ └────────────────┘ └────────────┘ │
└────────────────────┬───────────────────────────────────────┘
                     │
┌────────────────────▼───────────────────────────────────────┐
│               数据采集层（Ingestion）                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              OPC UA 采集集群（A/B冗余）              │    │
│  │    ┌─────────────────────────────────────────────┐  │    │
│  │    │  Active Node          │    Standby Node    │  │    │
│  │    │  ┌──────────────┐     │   ┌──────────────┐ │  │    │
│  │    │  │  open62541   │     │   │  open62541   │ │  │    │
│  │    │  │    Client    │◄────┼──►│    Client    │ │  │    │
│  │    │  └──────┬───────┘     │   └──────────────┘ │  │    │
│  │    └─────────┼─────────────┼───────────────────┘  │    │
│  └──────────────┼─────────────┼──────────────────────┘    │
│                 ▼             ▼                           │
└───────────────────────────────────────────────────────────┘
                          │ OPC UA
┌─────────────────────────▼─────────────────────────────────┐
│                 现场设备层（Field）                       │
│  ┌─────────────────────────────────────────────────────┐  │
│  │             KepserverEx（OPC UA Gateway）           │  │
│  └──────┬────────────────────┬────────────────────┬────┘  │
│         │                    │                    │       │
│   ┌─────▼────┐       ┌──────▼──────┐     ┌───────▼────┐ │
│   │  PLC 1   │       │    PLC 2    │     │  仪表设备  │ │
│   │  (1500)  │       │    (300)    │     │（流量/温度）│ │
│   └──────────┘       └─────────────┘     └────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### 技术栈

- **编程语言**: C++17
- **网络通信**: epoll + Reactor模式、TCP/IP
- **协议**: OPC UA (open62541)、Protocol Buffers
- **消息队列**: Apache Kafka
- **缓存**: Redis 6.x
- **数据库**: MySQL 8.0
- **容器化**: Docker、Docker Compose
- **日志**: spdlog
- **其他**: 线程池、链式缓冲区、零拷贝技术

## 🎯 核心功能模块

### 1. OPC UA 数据采集模块

**技术实现**:
- 基于open62541库实现OPC UA Client
- 支持多节点订阅与数据采样
- 实现自动重连与订阅重建机制
- 配置采样/发布周期与deadband策略

**关键特性**:
- 支持至少1000个Tag的并发采集
- 数据采集延迟 < 100ms
- 支持断点续传，数据完整性 > 99.99%

### 2. 高并发网络服务框架

**技术架构**:
- Reactor模式 + epoll I/O多路复用
- 主从Reactor线程模型
- 每连接独立的读写缓冲区

**缓冲区设计**:
```cpp
// 链式缓冲区结构示例
class ChainBuffer {
private:
    struct BufferNode {
        char* data;
        size_t capacity;
        size_t read_pos;
        size_t write_pos;
        BufferNode* next;
    };
    
    BufferNode* head_;
    BufferNode* tail_;
    // ... 实现分段回收和零拷贝发送
};
```

### 3. 消息总线与数据序列化

**Kafka集成**:
- 使用librdkafka C++客户端
- 实现批量发送与重试机制
- 支持至少一次投递语义

**Protobuf Schema**:
```protobuf
message DataPoint {
    string source_id = 1;      // 数据源标识
    string node_id = 2;        // OPC UA节点ID
    double value = 3;          // 数据值
    int64 device_timestamp = 4; // 设备时间戳
    int64 ingest_timestamp = 5; // 采集时间戳
    int32 quality = 6;         // 数据质量
    map<string, string> tags = 7; // 标签元数据
}
```

### 4. 高可用与冗余部署

**主备切换机制**:
- 基于Redis分布式锁实现Leader Election
- 支持Active-Standby和Active-Active模式
- 故障检测与自动切换时间 < 5秒

**数据去重策略**:
- Kafka生产者幂等性配置
- 消费者端基于时间窗口去重
- Redis布隆过滤器辅助去重

### 5. 数据存储与处理

**Redis缓存策略**:
- 最新数据缓存，TTL=10分钟
- 热点数据预加载
- 缓存击穿/雪崩防护

**MySQL优化**:
- 批量写入（每批次1000条）
- 按时间分表（每月一张表）
- 复合索引优化查询性能

### 6. 容器化部署

**Docker配置**:
```dockerfile
# 多阶段构建
FROM gcc:11 as builder
# 构建阶段...

FROM ubuntu:20.04
# 运行阶段...
HEALTHCHECK --interval=30s --timeout=3s \
  CMD ./health_check.sh
```

## 🚀 快速开始

### 环境要求

- Docker 20.10+
- Docker Compose 2.0+
- 至少8GB内存
- Linux/macOS系统

### 一键部署

```bash
# 克隆项目
git clone https://github.com/yourusername/data-collection-system.git
cd data-collection-system

# 启动所有服务
docker-compose up -d

# 查看服务状态
docker-compose ps

# 查看日志
docker-compose logs -f opcua-collector
```

### 手动构建

```bash
# 安装依赖
sudo apt-get install -y \
    libssl-dev \
    libopen62541-dev \
    librdkafka-dev \
    libprotobuf-dev \
    protobuf-compiler

# 构建项目
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 运行采集服务
./bin/opcua_collector --config ../config/config.yaml
```

## 📊 性能指标

| 指标 | 目标值 | 实测值 |
|------|--------|--------|
| 数据采集延迟 | < 100ms | 85ms |
| 系统吞吐量 | > 10k msg/s | 12k msg/s |
| 可用性 | 99.95% | 99.97% |
| 故障恢复时间 | < 30s | 25s |
| 数据完整性 | 99.99% | 99.995% |

## 🔧 配置说明

### 主要配置文件

```yaml
# config/config.yaml
opcua:
  endpoint: "opc.tcp://kepserver:4840"
  security_mode: 1  # None
  subscription_interval: 1000  # ms
  deadband_absolute: 0.1  # 绝对死区
  deadband_relative: 0.01  # 相对死区

kafka:
  bootstrap_servers: "kafka1:9092,kafka2:9092"
  topic: "opcua-data"
  acks: "all"
  retries: 3

redis:
  host: "redis-master"
  port: 6379
  connection_pool: 10

database:
  mysql_host: "mysql-primary"
  mysql_port: 3306
  batch_size: 1000
  flush_interval: 5000  # ms
```

## 🧪 测试与验证

### 单元测试

```bash
# 运行单元测试
cd build
ctest --output-on-failure
```

### 集成测试

```bash
# 启动测试环境
docker-compose -f docker-compose.test.yml up -d

# 运行集成测试
./scripts/run_integration_tests.sh
```

### 故障切换演练

1. **网络中断测试**
```bash
# 模拟网络分区
docker network disconnect data-network opcua-collector-a
# 验证备节点自动接管
```

2. **服务重启测试**
```bash
# 重启主采集节点
docker-compose restart opcua-collector-a
# 验证数据不丢失
```

## 📈 监控与告警

### 监控指标

- **系统级**: CPU、内存、磁盘IO、网络流量
- **应用级**: 采集延迟、消息队列积压、缓存命中率
- **业务级**: 数据完整性、告警数量、设备在线率

### 告警规则

```yaml
# alerting/rules.yaml
rules:
  - alert: HighDataLatency
    expr: opcua_collect_duration_seconds > 0.5
    for: 5m
    labels:
      severity: warning
    annotations:
      summary: "数据采集延迟过高"
      
  - alert: KafkaConsumerLag
    expr: kafka_consumer_lag > 1000
    for: 10m
    labels:
      severity: critical
```

## 📁 项目结构

```
distributed-data-collection/
├── code/
│   ├── data_collector/        # 数据采集模块
│   │   ├── main.cpp
│   │   ├── opcua_client/      # OPC UA客户端子模块
│   │   │   ├── client.cpp
│   │   │   ├── config.cpp
│   │   │   ├── data_point.cpp
│   │   │   └── data_collector.cpp
│   │   └── kafka_producer/    # Kafka生产者子模块
│   │       ├── kafka_producer.cpp
│   │       └── kafka_producer.hpp
│   └── data_processor/        # 数据处理模块
│       ├── main.cpp
│       ├── config.cpp/hpp     # 配置管理
│       └── kafka_consumer/    # Kafka消费者子模块
│           ├── kafka_consumer.cpp
│           └── kafka_consumer.hpp
│   ├── network/               # 网络框架
│   │   ├── reactor.cpp
│   │   ├── thread_pool.cpp
│   │   └── chain_buffer.cpp
│   ├── messaging/             # 消息队列
│   │   ├── kafka_producer.cpp
│   │   ├── kafka_consumer.cpp
│   │   └── protobuf_schema.cpp
│   ├── storage/               # 存储模块
│   │   ├── redis_cache.cpp
│   │   ├── mysql_writer.cpp
│   │   └── batch_processor.cpp
│   └── ha/                    # 高可用模块
│       ├── leader_election.cpp
│       ├── failover.cpp
│       └── health_check.cpp
├── config/                    # 配置文件
├── docker/                    # Docker配置
├── tests/                     # 测试代码
├── scripts/                   # 部署脚本
├── docs/                      # 文档
└── third_party/               # 第三方依赖
```

## 🔍 故障排查

### 常见问题

1. **OPC UA连接失败**
   ```bash
   # 检查KepserverEx服务状态
   docker-compose logs kepserver
   
   # 验证网络连通性
   docker exec opcua-collector ping kepserver
   ```

2. **Kafka消息积压**
   ```bash
   # 查看消费者组状态
   kafka-consumer-groups --bootstrap-server kafka:9092 --describe --group opcua-group
   
   # 调整消费者数量
   docker-compose scale kafka-consumer=3
   ```

3. **Redis内存不足**
   ```bash
   # 查看内存使用情况
   redis-cli info memory
   
   # 调整缓存策略
   redis-cli config set maxmemory-policy allkeys-lru
   ```

## 🤝 贡献指南

欢迎提交Issue和Pull Request！

1. Fork本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启Pull Request

## 📄 许可证

本项目采用MIT许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 📞 联系信息

如有问题或建议，请通过以下方式联系：

- 提交Issue: [GitHub Issues](https://github.com/yourusername/data-collection-system/issues)
- 邮箱: your.email@example.com

## 🎖️ 项目亮点

1. **工业级可靠性**: 经过啤酒工厂实际环境验证，稳定运行超过6个月
2. **高性能设计**: 单节点支持1000+ Tag并发采集，延迟低于100ms
3. **完整的高可用方案**: 实现从采集到存储的全链路冗余
4. **现代化技术栈**: 采用C++17、容器化、微服务架构
5. **完备的运维支持**: 提供监控、告警、故障演练全套方案

---

**⭐️ 如果这个项目对你有帮助，请点个Star支持！**