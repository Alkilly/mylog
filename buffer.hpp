/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 20:11:31
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-07 20:27:36
 * @FilePath: /project/buffer.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include <vector>     // std::vector
#include <algorithm>  // std::copy, std::swap
#include <cassert>    // assert
#include <cstddef>    // size_t
#ifndef __M_BUFFER_H__
#define __M_BUFFER_H__

namespace mylog{
class Buffer {
    public: //前端传送数据，后端写入磁盘
        // 容量策略常量（C++17 起 static constexpr 成员隐式 inline，无需类外定义）
        // static 属于类，不是对象：全局只有一份，不增加对象大小
        // constexpr 编译期常量，值在编译时就确定。
        static constexpr size_t DEFAULT_SIZE    = 1 * 1024 * 1024; // 初始容量 1MB
        static constexpr size_t INCREMENT_SIZE  = 1 * 1024 * 1024; // 超过阈值后每次步进 1MB
        static constexpr size_t THRESHOLD_SIZE  = 10 * 1024 * 1024; // 翻倍/步进分界 10MB
        Buffer(): _reader_idx(0), _writer_idx(0), _v(DEFAULT_SIZE){}
        bool empty() const { return _reader_idx == _writer_idx; }
        size_t readAbleSize() const { return _writer_idx - _reader_idx; }
        size_t writeAbleSize() const { return _v.size() - _writer_idx; }
        size_t capacity() const { return _v.size(); } // 当前容量（扩容后不回收，有意保留复用）
        void reset() { _reader_idx = _writer_idx = 0; }
        void swap(Buffer &buf)  
        {
            _v.swap(buf._v);
            std::swap(_reader_idx, buf._reader_idx);
            std::swap(_writer_idx, buf._writer_idx);
        }
        void push(const char *data, size_t len) { 
            ensureEnoughSpace(len);          
            assert(len <= writeAbleSize());  
            std::copy(data, data+len, &_v[_writer_idx]);
            _writer_idx += len;
        }
        const char*begin() { return &_v[_reader_idx]; }
        void pop(size_t len) 
        { 
            _reader_idx += len; 
            assert(_reader_idx <= _writer_idx);
        }
    protected:
        void ensureEnoughSpace(size_t len) {
            if (len <= writeAbleSize()) return;
            /*扩容策略：小容量翻倍（均摊 O(1)），大容量步进（避免翻倍浪费）*/ 
            size_t new_capacity;
            if (_v.size() < THRESHOLD_SIZE)
                new_capacity = _v.size() * 2 + len;
            else
                new_capacity = _v.size() + INCREMENT_SIZE + len;
            _v.resize(new_capacity);
        }
    private:
        size_t _reader_idx;
        size_t _writer_idx;
        std::vector<char> _v;
};
}
#endif
