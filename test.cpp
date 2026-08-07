/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-09 15:13:39
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-09 15:15:28
 * @FilePath: /project/logs_manage/test.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "mylog.h"


void loggerTest(const std::string &logger_name) {
    mylog::Logger::ptr lp = mylog::getLogger(logger_name);
    assert(lp.get());
    LOGF("------------example--------------------");
    LOG_DEBUG(lp, "%s", "LOG_DEBUG");
    LOG_INFO(lp, "%s", "LOG_INFO");
    LOG_WARN(lp, "%s", "LOG_WARN");
    LOG_ERROR(lp, "%s", "LOG_ERROR");
    LOG_FATAL(lp, "%s", "LOG_FATAL");
    LOGF("---------------------------------------");
    
    std::string log_msg = "hello bitejiuyeke-";
    size_t count = 0;
    while(count < 1000000) {
        std::string msg = log_msg + std::to_string(count++);
        LOG_ERROR(lp,"%s",msg.c_str());
    }
}
void functional_test() {
    mylog::GlobalLoggerBuilder::ptr lbp(new mylog::GlobalLoggerBuilder);
    lbp->buildLoggerName("all_sink_logger");
    lbp->buildFormatter("[%d][%c][%f:%l][%p] %m%n");
    lbp->buildLoggerLevel(mylog::LogLevel::value::DEBUG);
    lbp->buildSink<mylog::StdoutSink>();
    lbp->buildSink<mylog::FileSink>("./logs/sync.log");
    lbp->buildSink<mylog::RollSink>("./logs/roll-", 10 * 1024 * 1024);
    lbp->buildLoggerType(mylog::Logger::Type::LOGGER_ASYNC);
    lbp->build(); 
    loggerTest("all_sink_logger");
}
int main()
{
    functional_test();
    return 0;
}
