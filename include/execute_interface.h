#pragma once
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/wait.h>

#include "execute_settings.h"
#include "file_methods.hpp"

class ExecuteInterface
{
public:
    virtual ~ExecuteInterface() = default;

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
     * 设置内存限制
     * @param mem_limit 单位Byte
     */
    void setProcLimit(rlim_t mem_limit)
    {
        struct rlimit rl;
        rl.rlim_cur = mem_limit; // 软限制
        rl.rlim_max = mem_limit; // 硬限制（普通用户不可逆降低）
        if (setrlimit(RLIMIT_AS, &rl) == -1)
        { // RLIMIT_AS 限制虚拟内存
            perror("setrlimit failed");
            exit(EXIT_FAILURE);
        }
    }

    /**
     * 获取虚拟内存峰值
     */
    size_t get_vm_peak(pid_t pid)
    {
        std::ifstream status("/proc/" + std::to_string(pid) + "/status");
        std::string line;
        while (std::getline(status, line))
        {
            if (line.rfind("VmPeak", 0) == 0)
            {
                size_t kb = std::stoul(line.substr(7));
                return kb / 1024; // 转换为MB
            }
        }
        return 0;
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

        for (auto &element : list)
        {
            if (!element.is_object())
                throw std::runtime_error("array element is not an object");

            auto item = element.begin();
            if (item == element.end())
                throw std::runtime_error("array element has no key-value pair");

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