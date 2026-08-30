/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 20:10:12
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-08 09:25:34
 * @FilePath: /project/looper.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*  实现一个异步日志工作轮询器（AsyncLooper）
    将高频的业务线程日志写入（CPU/内存操作）与耗时的磁盘 I/O（落盘操作）彻底解耦。
    业务线程只需把数据塞进内存缓冲区便能毫秒级返回，
    底层的阻塞写盘由一个独立的后台工作线程慢慢处理。
*/

#ifndef __M_LOOP_H__
#define __M_LOOP_H__

#include <memory>        // std::shared_ptr
#include <string>        // std::string
#include <thread>        // std::thread
#include <mutex>         // std::mutex, std::unique_lock
#include <atomic>        // std::atomic
#include <condition_variable> // std::condition_variable
#include <functional>    // std::function
#include <chrono>        // std::chrono::milliseconds
#include <csignal>       // std::signal / std::raise / sig_atomic_t
#include "common.hpp"    // AsyncLooperPtr 全局别名（自包含）
#include "buffer.hpp"

namespace mylog{
    // ====== 崩溃兑底：信号 → 紧急落盘 ======
    // 信号处理器里只能做 async-signal-safe 的事（写 volatile sig_atomic_t 是安全的），
    // 真正的 flush 由 AsyncLooper 后端线程轮询执行（每轮循环检查，最长延迟一个轮询周期）
    inline volatile std::sig_atomic_t g_crash_flush = 0; // 1 = 检测到崩溃信号
    inline volatile std::sig_atomic_t g_crash_sig = 0;   // 记录触发的信号编号

    inline void crashFlushHandler(int sig) {
        g_crash_sig = sig;
        g_crash_flush = 1; // 只置标志，不做任何重活
    }

    // 注册崩溃信号（用户在 main 里主动调用；不自动装，避免改变程序默认行为）
    inline void installCrashHandlers() {
        std::signal(SIGSEGV, crashFlushHandler);
        std::signal(SIGABRT, crashFlushHandler);
        std::signal(SIGTERM, crashFlushHandler);
        std::signal(SIGINT, crashFlushHandler);
    }

    class AsyncLooper {
        public:
        //纯类型写法 std::function<void(Buffer &)> 是业界最标准的惯用形式
        //遇到带变量名的写法只需把它当成纯注释性命名即可
            using Functor = std::function<void(Buffer &buffer)>;
            using ptr = AsyncLooperPtr;
        // 构造函数：启动后台“扫地”线程
            explicit AsyncLooper(Functor cb, std::chrono::milliseconds flush_interval = std::chrono::seconds(1))
                : _looper_callback(std::move(cb)),
                  _flush_interval(flush_interval), 
                  _running(true) 
                {
                    // 创建并立刻启动后台线程
                    _thread = std::thread(&AsyncLooper::worker_loop, this);
                }
            ~AsyncLooper() { stop(); }
            // 前端显式请求立即落盘：唤醒后端把当前积压处理掉（不等阈值/定时）
            void flush() {
                _con_cond.notify_one();
            }
            void stop()
            { 
                _running = false; 
                // 唤醒可能正在等待的前端/后端线程
                _con_cond.notify_all(); 
                _pro_cond.notify_all();
                
                // 确保后台线程彻底把最后一票活干完，安全退出
                if (_thread.joinable()) 
                {
                    _thread.join();
                }
            }
            // 生产者入口
            void push(const std::string &msg){
                if (!_running ) return;
                bool need_wake = false;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _pro_cond.wait(lock, [&]{ // 条件变量等待：直到写空间足够 或 缓冲区为空 或 正在停止
                        return _pro_buf.writeAbleSize() >= msg.size()|| _pro_buf.empty() || !_running; });  // 空缓冲直接放行，push 内部会 ensureEnoughSpace 扩容
                        if(!_running) return; // 打烊了，这条丢弃，不再入队 
                    _pro_buf.push(msg.c_str(), msg.size());// 塞入前端缓冲区（空间不足自动扩容）
                    // 数据量 ≥ 容量 1/4 才唤醒后端（热路径加速）；稀疏流量由后端 1s 定时兜底
                    need_wake = _pro_buf.readAbleSize() >= _pro_buf.capacity() / 4;
                }
                if (need_wake) _con_cond.notify_one(); // 后端线程仅1个
            }
        private:
            void worker_loop(){ //后台线程常驻的执行函数，分为加锁拿数据与锁外写数据两阶段
                while(true){
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        // 1. 信号崩溃优先处理
                        if (g_crash_flush) {
                            if (!_pro_buf.empty()) {
                                _pro_buf.swap(_con_buf); // 拿货，去锁外处理
                            } else {
                                // 已处理完：恢复默认信号处理并重新触发，
                                // 让程序按原生方式崩溃退出（保留 core dump）
                                _pro_cond.notify_all();
                                std::signal(g_crash_sig, SIG_DFL);
                                std::raise(g_crash_sig);
                                return;
                            }
                        } else if (!_running  && _pro_buf.empty()) { return; }// 正常打烊退出
                        // 2. 超时等待：有数据、打烊或定时到期（1s）
                        _con_cond.wait_for(lock,_flush_interval,[&]{
                            return !_pro_buf.empty() || !_running || g_crash_flush;
                        });
                        if (_pro_buf.empty()) continue; // 空转超时：不交换不回调 
                        // 3. 核心步骤：微秒级交换
                        _pro_buf.swap(_con_buf);
                    }
                    //4.唤醒可能阻塞的前端线程
                    _pro_cond.notify_all();
                    // 5. 锁外执行落盘，耗时 I/O 绝不阻塞前端 push
                    _looper_callback(_con_buf);
                    // 6. 重置消费缓冲区，准备下一轮
                    _con_buf.reset();
                }
                return;
            }
        private:
            Functor _looper_callback;             // 后端把数据落盘的“业务执行函数”
            std::chrono::milliseconds _flush_interval; // 定时兑底刷盘间隔（空缓冲时醒来只查谓词，不刷盘）
            std::mutex _mutex;                    // 唯一的全局互斥锁
            std::atomic<bool> _running;           // 原子性的运行标志
            std::condition_variable _pro_cond;    // 前端生产条件变量（等空缓冲/打烊）
            std::condition_variable _con_cond;    // 后端消费条件变量（等数据/打烊）
            Buffer _pro_buf;                      // 前端写缓冲（业务线程塞数据）
            Buffer _con_buf;                      // 后端读缓冲（落地线程写盘）
            std::thread _thread;                  // 底层执行工作的后台线程
            };
}

//当 _running == true 时，表示系统正在正常营业，前端负责运货，后端负责清货 。
//当 _running == false 时（通常是用户调用了 stop() 或者程序准备退出），表示系统下达了“准备打烊”的通知 。
#endif