/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 16:25:48
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-07 23:27:02
 * @FilePath: /project/sink.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __M_SINK_H__
#define __M_SINK_H__

#include <fstream>
#include <sstream>
#include <cassert>
#include <memory>
#include "util.hpp" // 包含跨平台文件操作

namespace mylog {

class RollPolicy {
public:
    static constexpr int BY_SIZE = 1 << 0; // 二进制 01：按大小滚动
    static constexpr int BY_TIME = 1 << 1; // 二进制 10：按时间滚动
};    

// 1. 抽象基类（Sink 零件的标准）

class LogSink {
public:
    // 这里建议直接用全局 common.hpp 里的定义，如果保留类内别名，定义一次基类的即可
    using ptr = std::shared_ptr<LogSink>;

    LogSink() = default;
    virtual ~LogSink() = default; // 完美的现代虚析构 
    
    // 纯虚函数，不同的子类控制不同的落地方向 
    virtual void log(const char *data, size_t len) = 0;
};


// 2. 标准输出落地类（屏幕）
class StdoutSink : public LogSink {
public:
    StdoutSink() = default;   
    void log(const char *data, size_t len) override {
        std::cout.write(data, len); // 高效流输出，零拷贝 
    }
};


// 3. 固定文件落地类
class FileSink : public LogSink {
public:
    // 单参数构造函数，无脑加 explicit 防隐式转换隐患 
    explicit FileSink(const std::string &filename) : _filename(filename) {
        // 利用现代 C++17 风格的 util 创建目录 
        util::file::create_directory(util::file::path(filename));
        _ofs.open(_filename, std::ios::binary | std::ios::app);
        assert(_ofs.is_open()); // 失败前置防御 
    }
    void log(const char *data, size_t len) override {
        _ofs.write(data, len);
        if (!_ofs.good()) {
            std::cerr << "日志文件写入失败: " << _filename << std::endl;
        }
    }

private:
    std::string _filename;
    std::ofstream _ofs;
};


// 4. 滚动文件落地类

class RollSink : public LogSink {
public:
    // 构造函数：默认给它按时间滚动（RollPolicy::BY_TIME）
    RollSink(std::string basename, size_t max_fsize, int policy = RollPolicy::BY_TIME)
        : _basename(std::move(basename)), _max_fsize(max_fsize), 
          _policy(policy), _cur_fsize(0), _file_cnt(0) {
        util::file::create_directory(util::file::path(_basename));
    }

    void log(const char *data, size_t len) override {
        initLogFile(len);
        _ofs.write(data, len);
        if (!_ofs.good()) {
            std::cerr << "滚动日志文件写入失败！" << std::endl;
        }
        _cur_fsize += len;
    }

private:
    // 时间是否改变
bool isTimeChanged() {
        time_t t = time(nullptr);
        struct tm lt{};
        localtime_r(&t, &lt); // 线程安全的系统调用 

        // ⚠️ 修复：首次运行判定不能用 _last_time == 0！
        // createFilename() 在第一条日志时就会把 _last_time 设为当前秒，
        // 导致这里永远走不到初始化分支，_last_mday 保持初始值 0，
        // 而 tm_mday 合法范围是 1~31，第一次真实比较必然误判“跨天”→ 无条件滚动一次。
        // 改用哨兵值 -1 判定（tm_mday 最小为 1，-1 绝不冲突）。
        if (_last_mday == -1) {
            _last_time = t;
            _last_mday = lt.tm_mday; // 记录今天是哪一天
            _last_hour = lt.tm_hour; // 记录当前是几点
            return false;
        }

        // 场景 A：【按天滚动】
        if (lt.tm_mday != _last_mday) {
            _last_mday = lt.tm_mday; // 更新天数基准
            _last_time = t;
            return true; // 触发跨天滚动！
        }

        /* 场景 B：【按小时滚动】
        if (lt.tm_hour != _last_hour) {
            _last_hour = lt.tm_hour; // 更新小时基准
            _last_time = t;
            return true; // 触发跨小时滚动！
        }
        */

        return false;
    }

    // 文件是否超大
    bool isSizeOverflow(size_t len) const {
        return _cur_fsize + len > _max_fsize;
    }

private:
    void initLogFile(size_t len) {
        bool need_roll = false;

        // 如果文件压根没打开，那不管什么策略，无脑先开一个
        if (!_ofs.is_open()) {
            need_roll = true;
        } else {
            // 【核心：位掩码校验】
            // 检查是否开启了“按大小滚动”并且大小真的爆了
            if ((_policy & RollPolicy::BY_SIZE) && isSizeOverflow(len)) {
                need_roll = true;
            }
            // 检查是否开启了“按时间滚动”并且时间真的变了
            if ((_policy & RollPolicy::BY_TIME) && isTimeChanged()) {
                need_roll = true;
            }
        }

        // 触发滚动，执行切换管道
        if (need_roll) {
            if (_ofs.is_open()) {
                _ofs.close();
            }
            std::string name = createFilename();
            _ofs.open(name, std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
            
            // 状态重置卫兵
            _cur_fsize = 0;
        }
    } 

    std::string createFilename() {
        time_t t = time(nullptr);
        struct tm lt{};
        localtime_r(&t, &lt); // 线程安全的系统调用 
        
        std::stringstream ss;
        ss << _basename;
        ss << lt.tm_year + 1900
           << (lt.tm_mon + 1 < 10 ? "0" : "") << lt.tm_mon + 1
           << (lt.tm_mday < 10 ? "0" : "") << lt.tm_mday
           << (lt.tm_hour < 10 ? "0" : "") << lt.tm_hour
           << (lt.tm_min < 10 ? "0" : "") << lt.tm_min
           << (lt.tm_sec < 10 ? "0" : "") << lt.tm_sec;
        
        // 如果当前时间戳和上一次切文件的一样，计数器自增；否则计数器归零
        if (t == _last_time) {
            _file_cnt++;
        } else {
            _last_time = t;
            _file_cnt = 0;
        }
        
        ss << "_" << _file_cnt << ".log"; // 生成类似于 base_20260707163025_1.log 的文件名
        return ss.str();
    }

private:
    std::string _basename;
    std::ofstream _ofs;
    size_t _max_fsize;
    size_t _cur_fsize;
    int _policy; // 存放位掩码组合策略
    time_t _last_time = 0;    // 记录上一次切文件的时间戳
    size_t _file_cnt = 0;     // 同一秒内的文件区分计数器
    int _last_mday = -1; // 专门用来盯住“天”的哨兵（-1 表示未初始化，tm_mday 最小为 1）
    int _last_hour = -1; // 专门用来盯住“小时”的哨兵（同上）
};


// 5. 完美的完美模版工厂（完美转发）

class SinkFactory {
public:
    // 工业级万能制造工厂：利用了 C++11 右值引用 && 与 std::forward 完美转发
    template<typename SinkType, typename ...Args>
    static LogSink::ptr create(Args &&...args) {
        // 彻底消灭显式 new，拥抱最高效安全的 make_shared 
        return std::make_shared<SinkType>(std::forward<Args>(args)...);
    }
};

} // namespace mylog
#endif