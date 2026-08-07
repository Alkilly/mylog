/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-06 20:59:00
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-09 19:29:39
 * @FilePath: /project/logs_manage/level.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __M_LEVEL_H__
#define __M_LEVEL_H__

#include <string_view>

namespace mylog{
class LogLevel{
    public:
        enum class value {
            UNKNOW = 0,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            FATAL,
            OFF
        };
        static std::string_view toString(LogLevel::value l){
            static constexpr std::string_view names[] = {
            "UNKNOW", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"
        };
        // 安全检查：如果数值越界，返回 UNKNOW
        size_t idx = static_cast<size_t>(l);
        if (idx >= sizeof(names) / sizeof(names[0])) {
            return names[0];
        }
        return names[idx];
        }
};
}
#endif