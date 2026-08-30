#ifndef __M_MSG_H__
#define __M_MSG_H__
#include "util.hpp"
#include "level.hpp"
#include "common.hpp"
#include <thread>

namespace mylog{
struct LogMsg {
    using ptr = LogMsgPtr;
    //保持一定顺序，定常在前，变长在后
    size_t _line;//行号
    size_t _ctime;//时间
    std::thread::id _tid;//线程ID
    LogLevel::value _level;//日志等级
    std::string _name;//日志器名称
    std::string _file;//文件名
    std::string _payload;//日志消息

    LogMsg(std::string_view name, std::string_view file, 
        size_t line, std::string &&payload, LogLevel::value level)
        : _line(line),
          _ctime(util::now()),
          _tid(std::this_thread::get_id()),
          _level(level),
          _name(name),          // string_view 转 string 会自动构造
          _file(file),
          _payload(std::move(payload)) 
        {}

};
}

#endif