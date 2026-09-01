#include "mylog/mylog.h"


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
    
    std::string log_msg = "hello new world";
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
