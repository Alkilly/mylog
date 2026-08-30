/*
 * @Author: Alkili 3495494393@qq.com
 * @Date: 2026-07-06 16:10:26
 * @LastEditors: Alkili 3495494393@qq.com
 * @LastEditTime: 2026-07-06 20:58:25
 * @FilePath: /project/logs_manage/util.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
//工具

/*
    通用功能类，与业务无关的功能实现 C++17现代风格
        1. 获取系统时间
        2. 获取文件大小
        3. 创建目录
        4. 获取文件所在目录
*/
#ifndef __M_UTIL_H__
#define __M_UTIL_H__

#include <iostream>
#include <string>
#include <ctime>
#include<filesystem>

namespace mylog{
    namespace util{
        // 1.日期 获取当前系统的时间戳（秒级）
        inline size_t now(){
            return static_cast<size_t>(std::time(nullptr));
        }
        namespace file {
            // 2.判断文件或目录是否存在
                inline bool exists(const std::string &name) {
                    return std::filesystem::exists(name);
                }
                // 3.获取文件所在的目录路径
                inline std::string path(const std::string &filepath) {
                    std::filesystem::path p(filepath);
                    auto parent = p.parent_path();
                    return parent.empty() ? "." : parent.string();
                }
               // 4.创建多级目录
                inline void create_directory(const std::string &path) { 
                    if (path.empty()) return ;
                    std::filesystem::create_directories(path);
                }
        };
    }
}
#endif
