# mylog

基于 C++17 的 header-only 高性能日志库（零第三方依赖，10 个头文件）。

同步/异步双模式 · 多级别过滤 · 控制台/固定文件/滚动文件三落地方向 · 信号安全崩溃兜底

## 特性

- **同步/异步双模式**：`SyncLogger` 业务线程直写；`AsyncLogger` 双缓冲生产者-消费者模型，业务线程只入队、后台线程落盘
- **双缓冲异步模型**：swap O(1) 只换指针与索引，锁内临界区仅 memcpy，IO 全在锁外——生产与消费真正并行
- **攒批 flush**：字节/条数阈值可配置（默认 1MB），减少落盘次数；稀疏流量 1s 定时兜底
- **崩溃兜底**：信号处理器只置 `volatile sig_atomic_t` 标志（async-signal-safe），后端线程轮询紧急落盘后重放崩溃保留 core dump（异步 10 万条 + SIGSEGV 实测一行不丢）
- **多落地方向**：StdoutSink / FileSink（故障自动重连 + 限频报错）/ RollSink（按大小/时间位掩码组合滚动）
- **可扩展**：新增落地方向只需继承 `LogSink`，配合模板完美转发工厂零改动接入

## 设计模式

| 模式 | 落点 |
| --- | --- |
| Builder（建造者） | `Logger::Builder` + Local/Global 两种装配策略 |
| Factory（工厂） | `SinkFactory::create`（模板 + 完美转发）、`Formatter::createItem`（注册制） |
| Singleton（单例） | `LoggerManager`（Meyers 单例，C++11 线程安全） |
| Strategy（策略多态） | `FormatItem` 家族（格式化）、`LogSink` 家族（落地） |
| 生产者-消费者 | `AsyncLooper` 双缓冲（mutex + 双条件变量） |

## 快速开始

```cpp
#include "mylog.h"

int main() {
    // 一键打到默认 root 日志器（屏幕输出）
    LOGI("hello %s, %d", "world", 42);

    // 建造者装配自定义异步日志器
    auto builder = std::make_unique<mylog::GlobalLoggerBuilder>();
    builder->buildLoggerName("app");
    builder->buildLoggerType(mylog::Logger::Type::LOGGER_ASYNC);
    builder->buildFormatter("%H:%M:%S [%t] [%l] [%c:%n] %m");
    builder->buildSink<mylog::FileSink>("./logs/app.log");
    builder->buildSink<mylog::RollSink>("./logs/roll-", 10 * 1024 * 1024, mylog::RollPolicy::BY_SIZE);
    auto logger = builder->build();

    mylog::installCrashHandlers();  // 崩溃兜底
    LOG_INFO(logger, "async log: %d", 1);
    return 0;
}
```

## 架构

```
mylog.h(宏) → logger.hpp(Logger/Sync/Async/Builder/Manager)
            → formatter.hpp(FormatItem 家族 + 状态机解析)
            → sink.hpp(Stdout/File/Roll + SinkFactory)
            → looper.hpp(AsyncLooper 双缓冲)
            → buffer.hpp → message/level/util/common
```

## 目录结构

| 文件 | 职责 |
| --- | --- |
| `mylog.h` | 对外宏（LOG_INFO 等），快捷获取 logger |
| `logger.hpp` | Logger 基类 / SyncLogger / AsyncLogger / Builder / LoggerManager |
| `formatter.hpp` | 格式化：FormatItem 零件家族 + 状态机解析 |
| `sink.hpp` | 落地：LogSink 家族 + 位掩码滚动策略 + 完美转发工厂 |
| `looper.hpp` | 异步调度：双缓冲 + 双条件变量 + 崩溃兜底 |
| `buffer.hpp` | 线性缓冲：扩容策略 / swap / pop |
| `message.hpp` `level.hpp` `util.hpp` `common.hpp` | 消息体 / 级别枚举 / 文件工具 / 全局别名 |

## 构建与测试

```bash
g++ -std=c++17 -O2 test.cpp -o test && ./test   # 功能测试（含 100 万条异步压测）
g++ -std=c++17 -O2 main.cpp -o demo && ./demo   # 示例
```

## License

[MIT](LICENSE)
