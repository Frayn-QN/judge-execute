#include <workspace/workspace.hpp>

#include "rabbitmq_worker.hpp"
#include "execute_settings.h"

#include "execute_interface.h"
#include "c_cpp_execute.hpp"
#include "java_execute.hpp"
#include "python_execute.hpp"
#include "lua_execute.hpp"
#include "verilog_execute.hpp"

void work_func(wsp::workspace *spc, json taskData);

int main()
{
    std::cout << getCurrentTime() << "Hello JudgeExecute!" << endl;

    RabbitMQPull mqWorker;

    // 线程池配置
    auto *spcPtr = new wsp::workspace();
    auto brh_id = spcPtr->attach(new wsp::workbranch);
    // 最小线程数 最大线程数 时间间隔
    auto spv_id = spcPtr->attach(new wsp::supervisor(10, 100, 1000));
    (*spcPtr)[spv_id].supervise((*spcPtr)[brh_id]);

    std::cout << getCurrentTime() << "Start to Listen!" << endl;

    while (true)
    {
        json taskData;
        try
        {
            if (!mqWorker.pullTaskData(taskData))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }
        catch (const AmqpClient::ChannelException &e)
        {
            cout << e.what() << endl;
            continue;
        }

        // 提交工作线程
        spcPtr->submit([spcPtr, taskData]
                       { work_func(spcPtr, taskData); });
    }

    delete spcPtr;
}

void work_func(wsp::workspace *spc, json taskData)
{
    ExecuteInterface *executeImpl = nullptr;
    std::string taskID = taskData["task"]["id"];
    std::cout << getCurrentTime() << "Deal with Task: " << taskID << endl;

    // 取出taskData.task.answer.language
    std::string lang = taskData["task"]["answer"]["language"];
    bool judge = taskData["task"]["answer"]["judge"];

    // 根据language字段选择对应的实例
    if (lang == "C" || lang == "C++")
        executeImpl = new C_CppExecute(spc, taskData);
    else if (lang == "Java")
        executeImpl = new JavaExecute(spc, taskData);
    else if (lang == "Python")
        executeImpl = new PythonExecute(spc, taskData);
    else if (lang == "Lua")
        executeImpl = new LuaExecute(spc, taskData);
    else if (lang == "Verilog")
        executeImpl = new VerilogExecute(spc, taskData);
    else
    {
        std::cerr << getCurrentTime() << "Unsupported language: " << lang << std::endl;
        return;
    }

    std::cout << getCurrentTime() << "Work with Task: " << taskID << endl;
    try
    {
        executeImpl->save();
        executeImpl->execute();
        if (judge == true)
            executeImpl->compare();
        // else // 简单测试任务
        //     executeImpl->output();
    }
    catch (time_limit_error &e)
    { // 超时错误
        taskData["task"]["status"] = "TLE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
        std::cerr << "超时错误: " << e.what() << std::endl;
    }
    catch (memory_limit_error &e)
    { // 内存超出
        taskData["task"]["status"] = "MLE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
        std::cerr << "内存超出: " << e.what() << std::endl;
    }
    catch (std::runtime_error &e)
    { // 运行时错误
        taskData["task"]["status"] = "RE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
        std::cerr << "运行时异常: " << e.what() << std::endl;
    }
    catch (const std::string &msg)
    { // 按未知错误处理
        taskData["task"]["status"] = "UKE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = msg;
        std::cerr << "字符串异常: " << msg << std::endl;
    }
    catch (const std::exception &e)
    { // 按未知错误处理
        taskData["task"]["status"] = "UKE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
        std::cerr << "标准异常: " << e.what() << std::endl;
    }
    catch (...)
    { // 未知错误
        taskData["task"]["status"] = "UKE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = "Unknown Error";
        std::cerr << "Unknown Error" << std::endl;
    }

    // 回送TaskData
    std::cout << getCurrentTime() << "Push back Task: " << taskID << endl;
    RabbitMQPush mqWorker;
    mqWorker.pushTaskData(taskData);

    std::cout << getCurrentTime() << "Finish Task: " << taskID << endl;
    // 释放指针
    delete executeImpl;
}