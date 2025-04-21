#pragma once
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#include "execute_settings.h"
#include "file_methods.hpp"

class ExecuteInterface
{
public:
    /**
     * @brief 保存到文件系统
     */
    virtual void save() = 0;

    /**
     * @brief 执行程序，生成输出文件
     */
    virtual void execute() = 0;

    /**
     * @brief 对比预期输出与执行输出
     */
    virtual void compare() = 0;

    // /**
    //  * @brief 结果直接返回结果
    //  */
    // virtual void output() = 0;

    /**
     * @brief 执行任务定时线程函数
     * @param timeLimit_ms 时间限制（毫秒）
     * @param pid 执行任务的子进程pid
     */
    void executeAlarm(int timeLimit_ms, std::atomic<pid_t> &pid)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeLimit_ms));
        if (pid.load() != -1)
        {
            kill(pid.load(), SIGTERM);
        }
    }

    /**
     * @brief 向指定目录下保存文件
     * @param list Json List
     * @param dirPath 父目录
     */
    void saveFromJsonList(json &list, fs::path dirPath)
    {
        if (!list.is_array())
            throw std::runtime_error("wrong json type: not array");

        for (auto &item : list.items())
        {
            std::string key = item.key();
            std::string value = item.value();

            fs::path filePath(dirPath / key);
            Base64::DecodeBase64ToFile(value, filePath);
        }
    }

    /**
     * @brief 逐行比较两文件内容
     */
    bool compareFilesByLine(const fs::path &file1Path, const fs::path &file2Path)
    {
        std::ifstream f1(file1Path), f2(file2Path);
        std::string line1, line2;

        while (std::getline(f1, line1) && std::getline(f2, line2))
        {
            if (line1 != line2)
            {
                return false;
            }
        }

        // 检查是否有一方未读完
        if (f1.eof() != f2.eof())
            return false;
        return true;
    }
};