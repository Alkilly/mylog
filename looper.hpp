/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 20:10:12
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-08 09:25:34
 * @FilePath: /project/looper.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __M_LOOP_H__
#define __M_LOOP_H__

#include <memory>
#include "buffer.hpp"

namespace mylog{
    class AsyncLooper {
        public:
            using Functor = std::function<void(Buffer &buffer)>;
            using ptr = AsyncLooperPtr;
        // 构造函数：启动后台“扫地”线程
            explicit AsyncLooper(Functor cb)
                : _looper_callback(std::move(cb)), 
                _running(true) {
                // 创建并立刻启动后台线程
                _thread = std::thread(&AsyncLooper::worker_loop, this);
            }
            ~AsyncLooper() { stop(); }
            void stop(){ 
                _running = false; 
            // 唤醒可能正在安全检查中、或在等待新数据的后端线程
            _pop_cond.notify_all(); 
            _push_cond.notify_all();
            
            // 确保后台线程彻底把最后一票活干完，安全退出
            if (_thread.joinable()) {
                _thread.join();
            }
            }
            void push(const std::string &msg){
                if (!_running ) return;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _push_cond.wait(lock, [&]{ 
                        return _tasks_push.writeAbleSize() >= msg.size()|| !_running; }); 
                    _tasks_push.push(msg.c_str(), msg.size());
                }
                _pop_cond.notify_all();
            }
        private:
            void worker_loop(){
                while(true){
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        if (!_running  && _tasks_push.empty()) { return; }
                        _pop_cond.wait(lock, [&]{ return !_tasks_push.empty() || !_running; });
                        _tasks_push.swap(_tasks_pop);
                    }
                    _push_cond.notify_all();
                    _looper_callback(_tasks_pop);
                    _tasks_pop.reset();
                }
                return;
            }
        private:
            Functor _looper_callback;             // 后端把数据落盘的“业务执行函数”
            std::mutex _mutex;                    // 唯一的全局互斥锁
            std::atomic<bool> _running;           // 原子性的运行标志
            std::condition_variable _push_cond;   // 前端安全保护条件变量
            std::condition_variable _pop_cond;    // 后端唤醒条件变量
            Buffer _tasks_push;                   // 前端缓冲区（给业务线程塞数据）
            Buffer _tasks_pop;                    // 后端缓冲区（给落地线程去写盘）
            std::thread _thread;                  // 底层执行工作的后台线程
            };
}

//当 _running == true 时，表示系统正在正常营业，前端负责运货，后端负责清货 。
//当 _running == false 时（通常是用户调用了 stop() 或者程序准备退出），表示系统下达了“准备打烊”的通知 。
#endif