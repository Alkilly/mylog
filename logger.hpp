/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-08 09:24:09
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-09 19:06:21
 * @FilePath: /project/logs_manage/logger.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __M_LOG_H__
#define __M_LOG_H__

#include <cstdarg>
// 引入你之前写好的各个核心零件
#include "formatter.hpp"
#include "sink.hpp"
#include "looper.hpp" // 包含我们刚搞懂的异步 AsyncLooper

namespace mylog {


// 1. 日志器基类 (Logger)

class Logger {
public:
    enum class Type { LOGGER_SYNC = 0, LOGGER_ASYNC };
    using ptr = LoggerPtr; 

    Logger(const std::string &name, 
           Formatter::ptr formatter,
           std::vector<LogSink::ptr> &sinks, 
           LogLevel::value level = LogLevel::value::DEBUG)
        : _name(name), _formatter(formatter), _level(level),
          _sinks(sinks.begin(), sinks.end()) {}

    virtual ~Logger() {}

    std::string loggerName() { return _name; }
    LogLevel::value loggerLevel() { return _level; }

    // 前端一律调用的 5 级输出接口（支持可变参数 printf 风格）
    void debug(const char *file, size_t line, const char *fmt, ...) {
        if (!shouldLog(LogLevel::value::DEBUG)) return;
        va_list al; va_start(al, fmt);
        log(LogLevel::value::DEBUG, file, line, fmt, al);
        va_end(al);
    }
    void info(const char *file, size_t line, const char *fmt, ...) {
        if (!shouldLog(LogLevel::value::INFO)) return;
        va_list al; va_start(al, fmt);
        log(LogLevel::value::INFO, file, line, fmt, al);
        va_end(al);
    }
    void warn(const char *file, size_t line, const char *fmt, ...) {
        if (!shouldLog(LogLevel::value::WARN)) return;
        va_list al; va_start(al, fmt);
        log(LogLevel::value::WARN, file, line, fmt, al);
        va_end(al);
    }
    void error(const char *file, size_t line, const char *fmt, ...) {
        if (!shouldLog(LogLevel::value::ERROR)) return;
        va_list al; va_start(al, fmt);
        log(LogLevel::value::ERROR, file, line, fmt, al);
        va_end(al);
    }
    void fatal(const char *file, size_t line, const char *fmt, ...) {
        if (!shouldLog(LogLevel::value::FATAL)) return;
        va_list al; va_start(al, fmt);
        log(LogLevel::value::FATAL, file, line, fmt, al);
        va_end(al);
    }

public:
    
    // 2. 嵌套建造者基类 (Builder)
    // 关注内容：日志名称 日志等级 打印格式 落地方式
    class Builder {
    public:
        // ⚠️ 嵌套类无法在 common.hpp 前置声明，ptr 别名只能在此定义（唯一的例外）
        using ptr = std::shared_ptr<Builder>;
        Builder() : _logger_type(Logger::Type::LOGGER_SYNC), _level(LogLevel::value::DEBUG) {}
        virtual ~Builder() {}

        void buildLoggerName(const std::string &name) { _logger_name = name; }
        void buildLoggerLevel(LogLevel::value level) { _level = level; }
        void buildLoggerType(Logger::Type type) { _logger_type = type; }
        void buildFormatter(const std::string &pattern) { _formatter = std::make_shared<Formatter>(pattern); }
        void buildFormatter(const Formatter::ptr &formatter) { _formatter = formatter; }
        
        template<typename SinkType, typename ...Args>
        void buildSink(Args &&...args) { 
            auto sink = SinkFactory::create<SinkType>(std::forward<Args>(args)...);
            _sinks.push_back(sink); 
        }
        
        virtual Logger::ptr build() = 0; // 留给子类实现的最终生产命令

    protected:
        Logger::Type _logger_type;
        std::string _logger_name;
        LogLevel::value _level;
        Formatter::ptr _formatter;
        std::vector<LogSink::ptr> _sinks;
    };

protected:
    bool shouldLog(LogLevel::value level) { return level >= _level; }
    
    // 核心公共动作：加工、拼接、调用多态落地
    void log(LogLevel::value level, const char *file, size_t line, const char *fmt, va_list al) {
        char *buf;
        int len = vasprintf(&buf, fmt, al);
        if (len < 0) {
            return;
        }
        std::string msg(buf, len);
        free(buf);

        LogMsg lm(_name, file, line, std::move(msg), level);
        std::stringstream ss;
        _formatter->format(ss, lm);
        
        // 【多态的关键】调用子类去走不同的路
        logIt(ss.str());
    }

    // 纯虚函数：逼子类去实现究竟怎么吐出这个字符串
    virtual void logIt(const std::string &msg) = 0;

protected:
    std::mutex _mutex;
    std::string _name;
    Formatter::ptr _formatter;
    std::atomic<LogLevel::value> _level;
    std::vector<LogSink::ptr> _sinks;
};


// 3. 同步日志器子类 (SyncLogger)

class SyncLogger : public Logger {
public:
    using ptr = SyncLoggerPtr;
    SyncLogger(const std::string &name, Formatter::ptr formatter, std::vector<LogSink::ptr> &sinks, LogLevel::value level = LogLevel::value::DEBUG)
        : Logger(name, formatter, sinks, level) {
        std::cout << LogLevel::toString(level) << " 同步日志器: " << name << " 创建成功...\n";
    }

private:
    // 同步落地：业务线程自己拿着锁，去把所有的目的地（写文件/写控制台）遍历写一遍
    virtual void logIt(const std::string &msg) override {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_sinks.empty()) return;
        for (auto &it : _sinks) {
            it->log(msg.c_str(), msg.size());
        }
    }
};


// 4. 异步日志器子类 (AsyncLogger)

class AsyncLogger : public Logger {
public:
    using ptr = AsyncLoggerPtr;
    AsyncLogger(const std::string &name, Formatter::ptr formatter, std::vector<LogSink::ptr> &sinks, LogLevel::value level = LogLevel::value::DEBUG)
        : Logger(name, formatter, sinks, level) {
        
        // 绑定我们之前写好的异步 looper
        // 后端线程拿到货之后，就会执行这个 lambda，加锁遍历 sinks 落地
        _looper = std::make_shared<AsyncLooper>([this](Buffer &buf) {
            std::unique_lock<std::mutex> lock(this->_mutex);
            if (this->_sinks.empty()) return;
            for (auto &it : this->_sinks) {
                it->log(buf.begin(), buf.readAbleSize());
            }
        });
        std::cout << LogLevel::toString(level) << " 异步日志器: " << name << " 创建成功...\n";
    }

private:
    // 异步落地：极其潇洒，出了基类的加工厂后，不写盘，直接丢给异步流水线，瞬间返回！
    virtual void logIt(const std::string &msg) override {
        _looper->push(msg);
    }

private:
    AsyncLooper::ptr _looper; // 独占的后台流水线小秘书
};


// 5. 具体的建造者子类 (LocalLoggerBuilder)

class LocalLoggerBuilder : public Logger::Builder {
public:
    virtual Logger::ptr build() override {
        // 兜底逻辑 1：没起名字就报错
        if (_logger_name.empty()) {
            std::cout << "日志器名称不能为空！！\n";
            abort();
        }
        // 兜底逻辑 2：没选格式，送一套默认的
        if (_formatter == nullptr) {
            _formatter = std::make_shared<Formatter>();
        }
        // 兜底逻辑 3：没有落地方向，默认去屏幕
        if (_sinks.empty()) {
            _sinks.push_back(std::make_shared<StdoutSink>());
        }

        // 核心装配逻辑：根据用户的选择，多态生产对应的产品
        if (_logger_type == Logger::Type::LOGGER_ASYNC) {
            return std::make_shared<AsyncLogger>(_logger_name, _formatter, _sinks, _level);
        } else {
            return std::make_shared<SyncLogger>(_logger_name, _formatter, _sinks, _level);
        }
    }
};

class LoggerManager {
public:
    // 获取全局唯一实例的静态接口
    //著名的梅耶斯单例：C++11保证线程安全且绝不泄漏
    static LoggerManager& getInstance() {
        static LoggerManager lm;
        return lm;
    }
    // 检查是否存在某个日志器
    bool hasLogger(const std::string &name) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _loggers.find(name) != _loggers.end();
    }

    // 强力插入新日志器
    void addLogger(const std::string &name, const Logger::ptr &logger) {
        std::unique_lock<std::mutex> lock(_mutex);
        _loggers[name] = logger;
    }

    Logger::ptr getLogger(const std::string &name) {
        std::unique_lock<std::mutex> lock(_mutex);
        // 1. 正常的哈希表查找
        auto it = _loggers.find(name);
        if (it != _loggers.end()) {
            return it->second; // 找到了，直接返回已有的高配/定制日志器
        }
        // 2. 没找到！使用 Builder 现生一个标准日志器
        std::cout << "[LoggerManager] 警告：未检测到日志器 [" << name << "]，正在自动为您创建标准版...\n";
        auto builder = std::make_unique<LocalLoggerBuilder>();
        builder->buildLoggerName(name);// 录入最基本的零件信息       
        builder->buildLoggerType(Logger::Type::LOGGER_ASYNC); // 大厂高并发项目通常在这里默认给异步

        Logger::ptr new_logger = builder->build();// 3. 一键总装
        _loggers[name] = new_logger; // 4. 登记
         
        return new_logger;
    }
 
    // 获取 root 日志器
    Logger::ptr rootLogger() {
        std::unique_lock<std::mutex> lock(_mutex);//保护引用计数
        return _root_logger;
    }

private:
    // 4. 单例模式的核心：私有化构造、析构、拷贝构造和赋值运算符
    LoggerManager() {
        // 在构造函数中，默认创建好 root 日志器
        // 创建默认的单例兜底日志器：名称为 root，默认是同步的
        // 避开 Builder 的相互依赖，直接使用 LocalLoggerBuilder 本地装配
        auto slb = std::make_unique<LocalLoggerBuilder>();
        slb->buildLoggerName("root");
        slb->buildLoggerType(Logger::Type::LOGGER_SYNC);
        
        _root_logger = slb->build();
        _loggers["root"] = _root_logger;    
}
    
    ~LoggerManager() = default;
    LoggerManager(const LoggerManager&) = delete;            // 禁止拷贝
    LoggerManager& operator=(const LoggerManager&) = delete; // 禁止赋值

private:
    std::mutex _mutex;                                         // 互斥锁，保证多线程查找/添加日志器时的安全
    Logger::ptr _root_logger;                                  // 默认的根日志器
    std::unordered_map<std::string, Logger::ptr> _loggers;     // 核心仓库：用哈希表管理所有的日志器
};


// 全局注册型建造者 (GlobalLoggerBuilder)
class GlobalLoggerBuilder : public Logger::Builder {
public:
    virtual Logger::ptr build() override {
        if (_logger_name.empty()) {
            std::cout << "日志器名称不能为空！！\n";
            abort();
        }

        // 1. 拦截重复注册
        if (LoggerManager::getInstance().hasLogger(_logger_name)) {
            std::cout << "日志器: " << _logger_name << " 已经存在，拒绝重复创建！\n";
            return LoggerManager::getInstance().getLogger(_logger_name);
        }

        // 2. 兜底默认设置
        if (_formatter == nullptr) {
            _formatter = std::make_shared<Formatter>();
        }
        if (_sinks.empty()) {
            _sinks.push_back(std::make_shared<StdoutSink>());
        }

        // 3. 根据类型动态多态分流生产
        Logger::ptr lp;
        if (_logger_type == Logger::Type::LOGGER_ASYNC) {
            lp = std::make_shared<AsyncLogger>(_logger_name, _formatter, _sinks, _level);
        } else {
            lp = std::make_shared<SyncLogger>(_logger_name, _formatter, _sinks, _level);
        }

        // 刚生产出来的 Logger，自动塞进全局单例管理器里注册登记！
        LoggerManager::getInstance().addLogger(_logger_name, lp);
        return lp;
    }
};


} 


#endif