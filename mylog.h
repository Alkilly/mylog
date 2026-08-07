/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-09 15:03:00
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-09 15:11:59
 * @FilePath: /project/logs_manage/mylog.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __M_BIT_H__
#define __M_BIT_H__
#include "logger.hpp"

namespace mylog {

// 快捷获取日志器的全局辅助函数
inline Logger::ptr getLogger(const std::string &name) {
    return LoggerManager::getInstance().getLogger(name);
}

inline Logger::ptr rootLogger() {
    return LoggerManager::getInstance().rootLogger();
}

// 宏族一：允许程序员指定用哪个 logger 打印

#define LOG_DEBUG(logger, fmt, ...) (logger)->debug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(logger, fmt, ...)  (logger)->info(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(logger, fmt, ...)  (logger)->warn(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(logger, fmt, ...) (logger)->error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(logger, fmt, ...) (logger)->fatal(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 宏族二：不传logger ，直接一键打到默认 root 屏幕

#define LOGD(fmt, ...) LOG_DEBUG(mylog::rootLogger(), fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_INFO(mylog::rootLogger(), fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_WARN(mylog::rootLogger(), fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG_ERROR(mylog::rootLogger(), fmt, ##__VA_ARGS__)
#define LOGF(fmt, ...) LOG_FATAL(mylog::rootLogger(), fmt, ##__VA_ARGS__)

} // namespace mylog

#endif