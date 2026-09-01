#ifndef __M_LEVEL_H__
#define __M_LEVEL_H__

#include <string_view>
#include <cstdint>   // uint8_t                                                                                                                                                      
#include <cctype>    // std::tolower                                                                                                                                                 
#include <algorithm> // std::equal 

namespace mylog{
class LogLevel{
public:
        // 显式指定底层类型为 1 字节无符号整数，节省内存
    enum class value : uint8_t 
    { 
        UNKNOWN = 0, DEBUG, INFO, WARN, ERROR, FATAL, OFF 
    };
    static std::string_view toString(LogLevel::value l)
    {
        static constexpr std::string_view names[] = 
        {
            "UNKNOWN", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"
        };
        // 安全检查：如果数值越界，返回 UNKNOWN
        size_t idx = static_cast<size_t>(l);
        if (idx >= std::size(names)) {
            return names[0];
        }
            return names[idx];
    }
            // 字符串转枚举（高效、不区分大小写）
    inline static LogLevel::value fromString(std::string_view str) 
    {
        static constexpr struct 
        {
            std::string_view name;
            LogLevel::value level;
        } 
        kv_pairs[] = 
        {
            {"UNKNOWN", LogLevel::value::UNKNOWN},
            {"DEBUG",  LogLevel::value::DEBUG},
            {"INFO",   LogLevel::value::INFO},
            {"WARN",   LogLevel::value::WARN},
            {"ERROR",  LogLevel::value::ERROR},
            {"FATAL",  LogLevel::value::FATAL},
            {"OFF",    LogLevel::value::OFF}
        };

        // 逐字符不区分大小写比较辅助 lambda
        auto iequals = [](std::string_view a, std::string_view b) 
        {
            return a.size() == b.size() && 
                   std::equal(a.begin(), a.end(), b.begin(), [](char ca, char cb) 
                   {
                       return std::tolower(static_cast<unsigned char>(ca)) == 
                              std::tolower(static_cast<unsigned char>(cb));
                   });
        };

        // 遍历比对
        for (const auto& entry : kv_pairs) 
        {
            if (iequals(str, entry.name)) 
            {
                return entry.level;
            }
        }

        // SS无法识别的字符串默认返回 UNKNOWN（防御性兜底）
        return LogLevel::value::UNKNOWN;
    }
};
}
#endif