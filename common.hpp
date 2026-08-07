/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 09:58:06
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-07 16:29:45
 * @FilePath: /project/logs_manage/common.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __M_COMMON_H__
#define __M_COMMON_H__

#include <memory>

namespace mylog {

    // 核心前置声明（告诉编译器这些类存在，先不要管细节）
    struct LogMsg;
    class FormatItem;
    class Formatter;
    class Logger; // 未来还会有的日志器
    class LogSink;

    // =========================================================
    // 🌍 全局唯一控制中心：整个项目所有核心智能指针全部在这里定义！
    // =========================================================
    using LogMsgPtr     = std::shared_ptr<LogMsg>;
    using FormatItemPtr = std::shared_ptr<FormatItem>;
    using FormatterPtr  = std::shared_ptr<Formatter>;
    using LoggerPtr     = std::shared_ptr<Logger>;
    using LogSinkptr    = std::shared_ptr<LogSink>;
} // namespace mylog

#endif