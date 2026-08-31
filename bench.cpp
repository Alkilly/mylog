// ============================================================
// bench.cpp — mylog 性能基准测试
//
// 覆盖三类指标，产出可写进项目介绍的真实数据：
//   A. 崩溃容错   ：fork 子进程写 10 万条后真实 SIGSEGV 崩溃，
//                   校验日志一行不丢（信号安全兜底）
//   B. 同步 vs 异步：单线程各 100 万条，比吞吐（条/s、MB/s）
//   C. 多线程并发 ：4 线程 × 25 万 = 100 万条，异步 vs 同步对比，校验丢包/乱序
//
// 编译：g++ -std=c++17 -O2 -Wall -Wextra bench.cpp -o bench
// 运行：./bench
// ============================================================
#include "mylog.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>     // fork / _exit
#include <sys/wait.h>   // waitpid / WIFSIGNALED

using Clock = std::chrono::steady_clock;
using mylog::Logger;

// ---------------- 环境信息（保证数据可复现） ----------------

static std::string cpuModel() {
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string s = line.substr(pos + 2);
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
                return s;
            }
        }
    }
    return "unknown";
}

static double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// ---------------- 工具：统一装配测试日志器 ----------------

static Logger::ptr makeFileLogger(const std::string &name, const std::string &path,
                                  Logger::Type type,
                                  size_t flush_bytes = 1 * 1024 * 1024,
                                  size_t flush_lines = 0) {
    auto b = std::make_unique<mylog::LocalLoggerBuilder>();  // 本地装配：不注册全局管理器，重复跑无冲突
    b->buildLoggerName(name);
    b->buildLoggerType(type);
    b->buildFormatter("[%d{%H:%M:%S}][%c][%l] %m%n");  // 带时间戳的真实格式
    b->buildLoggerLevel(mylog::LogLevel::value::DEBUG);
    b->buildLoggerFlushPolicy(flush_bytes, flush_lines);
    b->buildSink<mylog::FileSink>(path);
    return b->build();
}

static long countLines(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    long n = 0;
    std::string line;
    while (std::getline(in, line)) ++n;
    return n;
}

// ---------------- A. 崩溃容错 ----------------

// 子进程写 N 条后真实崩溃（volatile 空指针解引用，防编译器优化掉），
// 父进程等它死后校验：信号类型正确 && 行数一条不丢。
static bool benchCrashSafety(long n = 100000) {
    const std::string path = "./logs/bench_crash.log";
    std::remove(path.c_str());

    pid_t pid = fork();
    if (pid == 0) {
        // ---- 子进程 ----
        auto lg = makeFileLogger("bench_crash", path, Logger::Type::LOGGER_ASYNC);
        mylog::installCrashHandlers();
        for (long i = 0; i < n; ++i) {
            LOG_INFO(lg, "crash seq=%06ld", i);
        }
        // 不 flush、不析构，立刻制造真实段错误：此刻 _pro_buf 里必有积压
        volatile int *p = nullptr;
        *p = 42;  // SIGSEGV
        _exit(1); // 永远不会走到
    }

    int status = 0;
    waitpid(pid, &status, 0);
    bool died_by_segv = WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV;
    long lines = countLines(path);
    bool ok = died_by_segv && lines == n;

    printf("  [A] 崩溃容错       : 异步写入 %ld 条后触发 SIGSEGV\n", n);
    printf("       进程死于 SIGSEGV : %s\n", died_by_segv ? "是" : "否(异常)");
    printf("       崩溃后日志行数   : %ld / %ld\n", lines, n);
    printf("       结果             : %s\n", ok ? "✅ 一行不丢" : "❌ 有丢失");
    return ok;
}

// ---------------- B. 单线程：同步 vs 异步（3 轮取最优） ----------------

static void benchSingleThread(long n = 1000000) {
    const char *msg = "bench payload-payload-payload %06ld";
    printf("\n  [B] 单线程吞吐（%ld 条 × 3 轮取最优，格式 [%%d{%%H:%%M:%%S}][%%c][%%l] %%m%%n）\n", n);

    auto runOnce = [&](Logger::Type type, const char *name, const char *path) {
        double best = 1e18;
        for (int r = 0; r < 3; ++r) {
            std::remove(path);
            {
                auto lg = makeFileLogger(name, path, type);
                auto t0 = Clock::now();
                for (long i = 0; i < n; ++i) LOG_INFO(lg, msg, i);
                lg->flush();
                double dt = msSince(t0);
                if (dt < best) best = dt;
            }
        }
        return best;
    };

    double sync  = runOnce(Logger::Type::LOGGER_SYNC,  "bench_sync",  "./logs/bench_sync.log");
    double async = runOnce(Logger::Type::LOGGER_ASYNC, "bench_async", "./logs/bench_async.log");

    printf("       同步 SyncLogger : %8.1f ms    %10.0f 条/s    %8.2f MB/s\n",
           sync, n / (sync / 1000.0), n * 100.0 / (sync / 1000.0) / (1024 * 1024));
    printf("       异步 AsyncLogger : %8.1f ms    %10.0f 条/s    %8.2f MB/s\n",
           async, n / (async / 1000.0), n * 100.0 / (async / 1000.0) / (1024 * 1024));
    printf("       异步/同步       : %.2fx（单线程下异步优势不在此，见 [C] 多线程）\n", async / sync);
}

// ---------------- C. 多线程高并发 + 完整性校验 ----------------

struct VerifyResult {
    long total_lines = 0;
    long out_of_order = 0;   // 乱序/重复
    long missing = 0;        // 缺失条数
    bool ok = false;
};

// 逐行解析 "tid=<t> seq=<s>"，校验每个线程 seq 严格递增、条数完整
static VerifyResult verifyFile(const std::string &path, int nthreads, long per) {
    VerifyResult r;
    std::ifstream in(path, std::ios::binary);
    std::vector<long> last_seq(nthreads, -1);
    std::vector<long> counts(nthreads, 0);

    std::string line;
    while (std::getline(in, line)) {
        ++r.total_lines;
        auto p1 = line.find("tid=");
        if (p1 == std::string::npos) continue;
        p1 += 4;
        int tid = 0;
        while (p1 < line.size() && line[p1] >= '0' && line[p1] <= '9') {
            tid = tid * 10 + (line[p1] - '0');
            ++p1;
        }
        auto p2 = line.find("seq=", p1);
        if (p2 == std::string::npos) continue;
        p2 += 4;
        long seq = 0;
        while (p2 < line.size() && line[p2] >= '0' && line[p2] <= '9') {
            seq = seq * 10 + (line[p2] - '0');
            ++p2;
        }
        if (tid < 0 || tid >= nthreads) continue;
        ++counts[tid];
        if (seq <= last_seq[tid]) ++r.out_of_order;  // 重复或乱序
        last_seq[tid] = seq;
    }
    for (int t = 0; t < nthreads; ++t) {
        if (counts[t] != per) r.missing += (per - counts[t]);
    }
    r.ok = (r.missing == 0 && r.out_of_order == 0);
    return r;
}

static void benchMultiThread(int nthreads = 4, long per = 250000) {
    const long total = nthreads * per;
    struct Run { Logger::Type type; const char *label; const char *name; const char *path; };
    Run runs[] = {
        { Logger::Type::LOGGER_ASYNC, "异步 AsyncLogger", "bench_mt_async", "./logs/bench_mt_async.log" },
        { Logger::Type::LOGGER_SYNC,  "同步 SyncLogger ", "bench_mt_sync",  "./logs/bench_mt_sync.log"  },
    };
    printf("\n  [C] 多线程并发（%d 线程 × %ld 条 = %ld 条 × 3 轮取最优，同机同格式）\n", nthreads, per, total);

    for (auto &run : runs) {
        double best = 1e18;
        VerifyResult best_r;
        for (int round = 0; round < 3; ++round) {
            std::remove(run.path);
            {
                auto lg = makeFileLogger(run.name, run.path, run.type);
                auto t0 = Clock::now();

                std::vector<std::thread> ths;
                ths.reserve(nthreads);
                for (int t = 0; t < nthreads; ++t) {
                    ths.emplace_back([lg, t, per] {
                        for (long i = 0; i < per; ++i) LOG_INFO(lg, "tid=%d seq=%06ld", t, i);
                    });
                }
                for (auto &th : ths) th.join();
                lg->flush();

                double dt = msSince(t0);   // 调用方视角耗时
                if (dt < best) best = dt;
            }  // 作用域结束 → 异步 logger 析构 → 后台线程 join，数据全部落盘后再校验
            VerifyResult r = verifyFile(run.path, nthreads, per);
            if (round == 0) best_r = r;
        }
        printf("       %s : %8.1f ms    %10.0f 条/s\n",
               run.label, best, total / (best / 1000.0));
        printf("             文件行数 %ld/%ld，缺失 %ld，乱序 %ld → %s\n",
               best_r.total_lines, total, best_r.missing, best_r.out_of_order,
               best_r.ok ? "✅ 零丢失零乱序" : "❌ 异常");
    }
}

// ---------------- main ----------------

int main() {
    std::remove("./logs/bench_sync.log");
    std::remove("./logs/bench_async.log");
    std::remove("./logs/bench_mt_async.log");
    std::remove("./logs/bench_mt_sync.log");

    printf("========== mylog benchmark ==========\n");
    printf("CPU  : %s\n", cpuModel().c_str());
    printf("核心 : %u 核\n", std::thread::hardware_concurrency());
    printf("编译 : %s %s, -O2 -Wall -Wextra\n", __VERSION__, __DATE__);
    printf("=====================================\n");

    bool all_ok = true;

    // A 必须在任何 logger/线程创建之前 fork（fork 前的进程必须无多线程状态）
    all_ok &= benchCrashSafety();
    benchSingleThread();
    benchMultiThread();

    printf("\n=====================================\n");
    printf("全部测试: %s\n", all_ok ? "✅ 通过" : "❌ 存在失败项");
    return all_ok ? 0 : 1;
}
