#pragma once

#include <workspace/workspace.hpp>
#include <sys/wait.h>

#include "execute_interface.h"

class VerilogExecute : public ExecuteInterface
{
private:
    wsp::workspace *spc_ptr;
    json &taskData;
    json &task;
    std::string taskID;
    fs::path taskDir;

public:
    VerilogExecute(wsp::workspace *spc, json &taskData) : spc_ptr(spc),
                                                          taskData(taskData),
                                                          task(taskData["task"])
    {
        taskID = task["id"];
        taskDir = FILE_ROOT_PATH + taskID;

        if (!fs::exists(taskDir))
        { // 创建任务目录
            if (!fs::create_directories(taskDir))
                throw std::runtime_error("Cannot create directory");
        }
        else // 由于id的唯一性，理论上不触发
            throw std::runtime_error("Directory already exists");
    }

    ~VerilogExecute() override
    {
        // 移除任务目录
        fs::remove_all(taskDir);
    }

    void save() override
    {
        json execute = taskData["execute"];
        json in = taskData["example"];
        json expectation = taskData["expectation"];

        // 保存执行文件
        saveFromJsonList(execute, taskDir);

        // 保存样例输入
        fs::path dir_input(taskDir / "input");
        if (!fs::create_directories(dir_input))
            throw std::runtime_error("Cannot create directory");
        saveFromJsonList(in, dir_input);

        // 保存期望输出
        fs::path dir_expectation(taskDir / "expectation");
        if (!fs::create_directories(dir_expectation))
            throw std::runtime_error("Cannot create directory");
        saveFromJsonList(expectation, dir_expectation);

        // 创建输出目录
        fs::path dir_output(taskDir / "output");
        if (!fs::create_directories(dir_output))
            throw std::runtime_error("Cannot create directory");
    }

    void execute() override
    {
        int timeLimit = taskData["timeLimit"];     // ms
        int memoryLimit = taskData["memoryLimit"]; // MB
        int testCount = taskData["testCount"];

        // 多次执行
        for (int i = 1; i <= testCount; i++)
        {
            // 输入文件
            fs::path inputFile(taskDir / "input" / (std::to_string(i) + ".in"));
            // 输出文件，同上
            fs::path outputFile(taskDir / "output" / (std::to_string(i) + ".out"));

            // 执行参数（运行二进制程序）
            std::vector<const char *> args;
            args.push_back("vvp");
            args.push_back("main");
            args.push_back(("+INPUT_FILE=" + inputFile.string()).c_str());
            args.push_back(nullptr);

            // 创建管道用于从stderr获取运行失败信息
            int pipefd[2];
            if (pipe(pipefd) == -1)
            {
                throw std::runtime_error("Pipe failed");
            }

            // 用child_pid监控pid变化
            std::atomic<pid_t> child_pid = -1;

            // fork
            pid_t pid = fork();
            if (pid == -1)
            {
                close(pipefd[0]);
                close(pipefd[1]);
                throw std::runtime_error("Fork failed");
            }
            else if (pid == 0)
            { // 子进程
                chdir(taskDir.c_str());

                // 重定向
                close(pipefd[0]); // 关闭读端
                dup2(pipefd[1], STDERR_FILENO);
                close(pipefd[1]);
                freopen(outputFile.c_str(), "w", stdout);

                // 环境隔离
                unshare(CLONE_NEWPID | CLONE_NEWNS);

                // 执行
                execvp("vvp", const_cast<char *const *>(args.data()));

                // execvp 失败
                std::cerr << "执行命令失败" << std::endl;
                _exit(1);
            }
            else if (pid > 0)
            {                     // 父进程
                close(pipefd[1]); // 关闭写端
                child_pid.store(pid);

                // 设置定时任务
                spc_ptr->submit([this, timeLimit, &child_pid]
                                { executeAlarm(timeLimit, child_pid); });

                int status;
                if (waitpid(pid, &status, 0) > 0)
                { // 等待子进程执行结束
                    child_pid.store(-1);
                }

                // 读取错误信息
                char buffer[1024];
                std::string executeError;
                ssize_t bytesRead;
                while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
                {
                    buffer[bytesRead] = '\0';
                    executeError += buffer;
                }
                close(pipefd[0]);

                if (WIFSIGNALED(status))
                {                     // 信号中止
                    close(pipefd[0]); // 退出前关闭读端

                    int sig = WTERMSIG(status);
                    if (sig == SIGTERM)
                    { // 超时
                        throw time_limit_error("time out!");
                    }
                    else if (sig == SIGSEGV)
                    {
                        if (get_vm_peak(pid) >= memoryLimit) // 超内存
                            throw memory_limit_error("memory out of limit!");
                        else
                            throw runtime_error(executeError);
                    }
                    else if (sig == SIGKILL)
                    { // oom结束进程
                        throw memory_limit_error("kill by oom!");
                    }
                    // 未知错误
                    throw runtime_error("signal:" + std::to_string(sig));
                }

                if (!executeError.empty())
                {
                    throw runtime_error(executeError);
                }
            }
        }
    }

    void compare() override
    {
        int testCount = taskData["testCount"];
        int acceptCount = 0, wrongCount = 0;
        for (int i = 1; i <= testCount; i++)
        {
            fs::path outputPath(taskDir / "output" / (std::to_string(i) + ".out"));
            fs::path expectationPath(taskDir / "expectation" / (std::to_string(i) + ".out"));

            if (compareFilesByLine(outputPath, expectationPath))
                acceptCount++;
            else
                wrongCount++;
        }

        json *result = &task["result"];
        (*result)["total"] = testCount;
        (*result)["accept"] = acceptCount;
        (*result)["wrong"] = wrongCount;
        task["score"] = 1.0 * acceptCount / testCount;

        if (wrongCount == 0)
            task["status"] = "AC";
        else
            task["status"] = "WA";
    }
};