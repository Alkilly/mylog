/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 09:58:06
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-07 16:29:45
 * @FilePath: /project/logs_manage/common.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BB%AE
 */
#ifndef __M_COMMON_H__
#define __M_COMMON_H__

#include <memory>

namespace mylog {

    // 核心前置声明（告诉编译器这些类存在，先不要管细节）
    struct LogMsg;
    class FormatItem;
    class Formatter;
    class Logger;
    class LogSink;
    class AsyncLooper;
    class SyncLogger;
    class AsyncLogger;

    // =========================================================
    // 🌍 全局唯一控制中心：整个项目所有核心智能指针全部在这里定义！
    // 各类的类内别名 (using ptr = XxxPtr) 统一引用这里的全局别名，
    // 将来想换智能指针类型（如 intrusive_ptr）只改这一处。
    //
    // ⚠️ 唯一的例外：Logger::Builder 是 Logger 的嵌套类，
    // 嵌套类无法在 common.hpp 前置声明（需要 Logger 完整定义），
    // 所以 Builder::ptr 只能在 logger.hpp 内定义。
    // =========================================================
    using LogMsgPtr      = std::shared_ptr<LogMsg>;
    using FormatItemPtr  = std::shared_ptr<FormatItem>;
    using FormatterPtr   = std::shared_ptr<Formatter>;
    using LoggerPtr      = std::shared_ptr<Logger>;
    using LogSinkPtr     = std::shared_ptr<LogSink>;
    using AsyncLooperPtr = std::shared_ptr<AsyncLooper>;
    using SyncLoggerPtr  = std::shared_ptr<SyncLogger>;
    using AsyncLoggerPtr = std::shared_ptr<AsyncLogger>;
} // namespace mylog

#endif
