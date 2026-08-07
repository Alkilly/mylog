/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-07 20:11:31
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-07 20:27:36
 * @FilePath: /project/buffer.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <cassert>
namespace mylog{
#define BUFFER_DEFAULT_SIZE (1*1024*1024)
#define BUFFER_INCREMENT_SIZE (1*1024*1024)
#define BUFFER_THRESHOLD_SIZE (10*1024*1024)
class Buffer {
    public: //前端传送数据，后端写入磁盘
        Buffer(): _reader_idx(0), _writer_idx(0), _v(BUFFER_DEFAULT_SIZE){}
        bool empty() { return _reader_idx == _writer_idx; }
        size_t readAbleSize() { return _writer_idx - _reader_idx; }
        size_t writeAbleSize() { return _v.size() - _writer_idx; }
        void reset() { _reader_idx = _writer_idx = 0; }
        void swap(Buffer &buf)  {
            _v.swap(buf._v);
            std::swap(_reader_idx, buf._reader_idx);
            std::swap(_writer_idx, buf._writer_idx);
        }
        void push(const char *data, size_t len) { 
            assert(len <= writeAbleSize());
            ensureEnoughSpace(len);
            std::copy(data, data+len, &_v[_writer_idx]);
            _writer_idx += len;
        }
        const char*begin() { return &_v[_reader_idx]; }
        void pop(size_t len) { 
            _reader_idx += len; 
            assert(_reader_idx <= _writer_idx);
        }
    protected:
        void ensureEnoughSpace(size_t len) {
            if (len <= writeAbleSize()) return;
            /*每次增大1M大小*/
            size_t new_capacity;
            if (_v.size() < BUFFER_THRESHOLD_SIZE)
                new_capacity = _v.size() * 2 + len;
            else
                new_capacity = _v.size() + BUFFER_INCREMENT_SIZE + len;
            _v.resize(new_capacity);
        }
    private:
        size_t _reader_idx;
        size_t _writer_idx;
        std::vector<char> _v;
};
}