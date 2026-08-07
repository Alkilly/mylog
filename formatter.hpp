//日志格式化器
//支持用户自定义日志输出格式

// 使用派生类的目的（要素拆解）：
// 我们将整个复杂的日志格式，拆解成了一个个独立的“零件”
// MsgFormatItem 专门负责打印日志内容（msg._payload） 
// LevelFormatItem 专门负责打印级别（msg._level）
// TimeFormatItem 专门负责计算并打印时间
// 当程序启动解析完格式后， Formatter 内部其实就存了一个零件数组（std::vector<FormatItem::ptr>） 

#ifndef __M_FMT_H__
#define __M_FMT_H__

#include "message.hpp"
#include <vector>
#include <sstream>
#include <cassert>
#include <mutex> // 引入互斥锁以保证多线程安全

namespace mylog {

// 1. 格式化子项基类 
class FormatItem {
public:
    using ptr = FormatItemPtr; // 类型别名，方便外部使用智能指针
    virtual ~FormatItem() = default;         // 现代 C++ 虚析构函数写法
    virtual void format(std::ostream &os, const LogMsg &msg) = 0; // 纯虚函数，供子类实现多态 
};

// 2. 各要素的具体派生类 

// 消息正文组件
class MsgFormatItem : public FormatItem {
public:
    MsgFormatItem() = default; // 不需要参数的类直接使用默认构造 
    void format(std::ostream &os, const LogMsg &msg) override {
        os << msg._payload; // 从消息体取出日志有效载荷
    }
};

// 日志级别组件
class LevelFormatItem : public FormatItem {
public:
    LevelFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << LogLevel::toString(msg._level); 
    }
};

// 日志器名称组件
class NameFormatItem : public FormatItem {
public:
    NameFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << msg._name;
    }
};

// 线程 ID 组件
class ThreadFormatItem : public FormatItem {
public:
    ThreadFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << msg._tid; 
    }
};

// 源码文件名组件
class CFileFormatItem : public FormatItem {
public:
    CFileFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << msg._file;
    }
};

// 源码行号组件
class CLineFormatItem : public FormatItem {
public:
    CLineFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << msg._line; 
    }
};

// 制表符缩进组件 (\t)
class TabFormatItem : public FormatItem {
public:
    TabFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << "\t"; 
    }
};

// 换行符组件 (\n)
class NLineFormatItem : public FormatItem {
public:
    NLineFormatItem() = default;
    void format(std::ostream &os, const LogMsg &msg) override {
        os << "\n";
    }
};

// 原始字符串组件（用于输出非 % 标记的普通中括号、空格、冒号等）
class OtherFormatItem : public FormatItem {
private:
    std::string _str;
public:
    explicit OtherFormatItem(std::string str) : _str(std::move(str)) {} // 采用 std::move 避免拷贝
    void format(std::ostream &os, const LogMsg &msg) override {
        os << _str; 
    }
};

//防止1s内重复使用localtime_r系统调用
class TimeFormatItem : public FormatItem {
private:
    std::string _format;
    
    // ====== 核心优化缓存字段 ======
    // mutable 使 即使在const函数里，成员函数也能被修改
    mutable std::mutex _mutex;           // 保护缓存的互斥锁（注意加上 mutable）
    mutable time_t _last_ctime = 0;      // 记录上一次成功转换的时间戳（秒数）
    mutable std::string _cached_str;     // 存储上一次格式化好的时间字符串

public:
    explicit TimeFormatItem(std::string format = "%H:%M:%S") : _format(std::move(format)) {
        if (_format.empty()) _format = "%H:%M:%S"; 
    }

    void format(std::ostream &os, const LogMsg &msg) override {
        time_t current_time = msg._ctime; // 拿到当前日志的时间戳

        {
            // 1. 使用 C++ 的 lock_guard 自动加锁，保证多线程安全
            std::lock_guard<std::mutex> lock(_mutex);

            // 2. 作弊判定：如果当前秒数和上一次记录的秒数完全相同
            if (current_time == _last_ctime && !_cached_str.empty()) {
                os << _cached_str; // 直接复用上次的字符串！免去所有昂贵的计算！
                return;            // 闪电结束
            }

            // 3. 如果步入了新的一秒（或者第一次运行），则老老实实进行转换
            _last_ctime = current_time; // 更新计时器为当前秒数

            struct tm lt{};
            localtime_r(&current_time, &lt); // 安全转换
            
            char tmp[128];
            strftime(tmp, sizeof(tmp), _format.c_str(), &lt);
            
            _cached_str = tmp; // 将精心计算的结果塞入缓存，供同秒内后续兄弟们享用
        }

        // 4. 输出本次转换的结果
        os << _cached_str;
    }
};

// 3. 日志格式化控制核心类 
class Formatter {
public:
    using ptr = FormatterPtr; 
    // 默认日志输出格式
    explicit Formatter(std::string pattern = "[%d{%H:%M:%S}][%t][%p][%c][%f:%l] %m%n")
        : _pattern(std::move(pattern)) {
        assert(parsePattern()); // 初始化时立即解析格式串 
    }

    std::string pattern() const { return _pattern; }

    // 重载 A：将日志直接格式化并打包返回成字符串（内部存在 ss.str() 深拷贝） 
    std::string format(const LogMsg &msg) {
        std::stringstream ss;
        format(ss, msg); // 复用重载 B
        return ss.str(); //
    }

    // 重载 B：高效的流式格式化，直接输出到对应的目标流（推荐，零拷贝）
    std::ostream& format(std::ostream &os, const LogMsg &msg) {
        for (const auto &it : _items) {
            it->format(os, msg); // 多态性调用：自动去匹配不同的具体子类执行 
        }
        return os; 
    }

private:
    // 内部结构体：替代原先晦涩的 std::tuple，极大提升可读性 
    struct FormatNode {
        std::string key; // 格式化字符（如 "d", "m" 等）
        std::string val; // 对应的花括号子格式串（如 "%H:%M:%S"）
        int type;        // 0-原始普通字符串，1-格式化标签 
    };

    // 工厂模式核心函数：消灭 new，全部平替为标准的 make_shared 内存模型 
    FormatItem::ptr createItem(const std::string &fc, const std::string &subfmt) {
        if (fc == "m") return std::make_shared<MsgFormatItem>(); 
        if (fc == "p") return std::make_shared<LevelFormatItem>(); 
        if (fc == "c") return std::make_shared<NameFormatItem>(); 
        if (fc == "t") return std::make_shared<ThreadFormatItem>(); 
        if (fc == "n") return std::make_shared<NLineFormatItem>(); 
        if (fc == "f") return std::make_shared<CFileFormatItem>(); 
        if (fc == "l") return std::make_shared<CLineFormatItem>(); 
        if (fc == "T") return std::make_shared<TabFormatItem>(); 
        if (fc == "d") return std::make_shared<TimeFormatItem>(subfmt); // 时间组件单独传参子格式
        return nullptr; 
    }

    // pattern解析核心引擎（状态机逻辑简化）
    bool parsePattern() {
        std::vector<FormatNode> tokens; // 存放解析后的中间节点 
        
        std::string format_key;
        std::string format_val;
        std::string string_row;
        bool sub_format_error = false;
        size_t pos = 0;

        while (pos < _pattern.size()) {
            // 1. 处理普通非格式化字符 
            if (_pattern[pos] != '%') {
                string_row.append(1, _pattern[pos++]); // 
                continue; // 
            }
            // 2. 处理转义的双百分号 %%
            if (pos + 1 < _pattern.size() && _pattern[pos + 1] == '%') {
                string_row.append(1, '%'); // 
                pos += 2; // 
                continue; // 
            }
            // 3. 遇到了单 % 号：说明一段普通文本结束，要开始存入节点 
            if (!string_row.empty()) {
                tokens.push_back({string_row, "", 0}); // 结构体直接初始化，省去 make_tuple 
                string_row.clear(); // 
            }

            pos += 1; // 跨过 '%'，指向格式化字符（如 d, m 等） 
            if (pos < _pattern.size() && std::isalpha(_pattern[pos])) {
                format_key = _pattern[pos]; // 
            } else {
                std::cout << "错误: " << _pattern.substr(pos == 0 ? 0 : pos - 1) << " 附近格式解析失败！\n";
                return false; // 
            }

            pos += 1; // 跨过格式化字符，判断其后是否紧跟花括号子格式 {} 
            if (pos < _pattern.size() && _pattern[pos] == '{') {
                sub_format_error = true; // 
                pos += 1; // 跨过 '{' 
                while (pos < _pattern.size()) {
                    if (_pattern[pos] == '}') {
                        sub_format_error = false; // 
                        pos += 1; // 跨过 '}' 
                        break; // 
                    }
                    format_val.append(1, _pattern[pos++]); // 填充子格式串 
                }
            }

            tokens.push_back({format_key, format_val, 1}); // 归档格式化要素 
            format_key.clear(); // 
            format_val.clear(); // 
        }

        if (sub_format_error) {
            std::cout << "错误: 格式化花括号 {} 闭合不匹配！\n";
            return false; // 
        }

        // 扫尾工作
        if (!string_row.empty()) tokens.push_back({string_row, "", 0}); // 
        if (!format_key.empty()) tokens.push_back({format_key, format_val, 1}); // 

        // 4. 将提取出的中间节点通过多态工厂转化为真正的零件数组并拼装 
        for (auto &token : tokens) {
            if (token.type == 0) {
                _items.push_back(std::make_shared<OtherFormatItem>(token.key)); // 
            } else {
                auto item = createItem(token.key, token.val); // 
                if (item == nullptr) {
                    std::cout << "错误: 无法识别的格式化指令 %" << token.key << "\n";
                    return false;
                }
                _items.push_back(item);
            }
        }
        return true;
    }

private:
    std::string _pattern;               // 格式规则存储串 
    std::vector<FormatItem::ptr> _items; // 零件对象数组 
};

} // namespace mylog

#endif // __M_FMT_H__