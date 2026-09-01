#include "mylog/logger.hpp" // 包含刚才总装厂的头文件

int main() {
    // =================================================================
    // 阶段一：使用【建造者模式】去组装一个【异步日志器】
    // =================================================================
    
    // 1. 声明一个具体的总装车间顾问 (Builder)
    mylog::Logger::Builder::ptr builder = std::make_shared<mylog::LocalLoggerBuilder>();
    
    // 2. 像流水线点单一样，往 builder 这个“小本本”上记录我们想要的配置
    builder->buildLoggerName("MyAsyncServer");                     // 跑车颜色：名字
    builder->buildLoggerLevel(mylog::LogLevel::value::INFO);      // 引擎大小：INFO级别以上才打印
    builder->buildLoggerType(mylog::Logger::Type::LOGGER_ASYNC);  // 核心动力：我要高性能异步模式！！
    builder->buildFormatter("[%d{%Y-%m-%d %H:%M:%S}][%p][%c] %m%n"); // 仪表盘：日志格式
    
    // 3. 给这个日志器添加两个落地方向 (Sinks)
    // 零件 A：打到控制台屏幕
    builder->buildSink<mylog::StdoutSink>(); 
    // 零件 B：同时写入到指定的文件中
    builder->buildSink<mylog::FileSink>("./server.log"); 

    // 4. 啪！一键交付！调用 build()。
    // 这时候 LocalLoggerBuilder 内部会判断 _logger_type == LOGGER_ASYNC，
    // 于是默默 new 了一个 AsyncLogger，并当成大基类指针 Logger::ptr 返回给我们。
    mylog::Logger::ptr my_logger = builder->build();

    std::cout << "--- 建造者组装完毕，开始进入业务线 ---\n\n";

    // =================================================================
    // 阶段二：前端业务线程开始疯狂打印日志（多态与异步的运转）
    // =================================================================
    
    for (int i = 0; i < 3; ++i) {
        // 5. 业务线程调用 info 接口
        my_logger->info(__FILE__, __LINE__, "这是第 %d 条用户请求，连接状态: %s", i, "成功");
    }

    // 歇一会儿，给后台扫地线程一点落盘的时间，然后安全退出
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 0;
}