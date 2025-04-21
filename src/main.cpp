#include <workspace/workspace.hpp>

#include "rabbitmq_worker.hpp"
#include "execute_settings.h"

#include "execute_interface.h"
#include "c_cpp_execute.hpp"
#include "java_execute.hpp"
#include "python_execute.hpp"
#include "lua_execute.hpp"
#include "verilog_execute.hpp"

void work_func(wsp::workspace *spc, json &taskData);

int main()
{
    cout << "Hello JudgeExecute!" << endl;

    RabbitMQPull mqWorker;

    // 线程池配置
    auto *spcPtr = new wsp::workspace();
    auto brh_id = spcPtr->attach(new wsp::workbranch);
    // 最小线程数 最大线程数 时间间隔
    auto spv_id = spcPtr->attach(new wsp::supervisor(10, 100, 1000));
    (*spcPtr)[spv_id].supervise((*spcPtr)[brh_id]);

    cout << "Start to Listen!" << endl;

    while (true)
    {
        json taskData;
        if (!mqWorker.pullTaskData(taskData))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 提交工作线程
        spcPtr->submit([spcPtr, &taskData]
                       { work_func(spcPtr, taskData); });
    }

    delete spcPtr;
}

void work_func(wsp::workspace *spc, json &taskData)
{
    ExecuteInterface *executeInterface;
    std::string taskID = taskData["task"]["id"];
    std::cout << "Deal with Task: " << taskID << endl;

    // 取出taskData.task.answer.language
    std::string lang = taskData["task"]["answer"]["language"];
    bool judge = taskData["task"]["answer"]["judge"];

    // 根据language字段选择对应的实例
    if (lang == "C" || lang == "C++")
        executeInterface = new C_CppExecute(spc, taskData);
    else if (lang == "Java")
        executeInterface = new JavaExecute(spc, taskData);
    else if (lang == "Python")
        executeInterface = new PythonExecute(spc, taskData);
    else if (lang == "Lua")
        executeInterface = new LuaExecute(spc, taskData);
    else if (lang == "Verilog")
        executeInterface = new VerilogExecute(spc, taskData);
    else
    {
        std::cerr << "Unsupported language: " << lang << std::endl;
        return;
    }

    std::cout << "Work with Task: " << taskID << endl;
    try
    {
        executeInterface->save();
        executeInterface->execute();
        if (taskData["judge"] == true)
            executeInterface->compare();
        // else // 简单测试任务
        //     executeInterface->output();
    }
    catch (time_limit_error &e)
    { // 超时错误
        taskData["task"]["status"] = "TLE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
    }
    catch (memory_limit_error &e)
    { // 内存超出
        taskData["task"]["status"] = "MLE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
    }
    catch (std::runtime_error &e)
    { // 运行时错误
        taskData["task"]["status"] = "RE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = e.what();
    }
    catch (const std::string &msg)
    { // 按未知错误处理
        taskData["task"]["status"] = "UKE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = msg;
    }
    catch (...)
    { // 未知错误
        taskData["task"]["status"] = "UKE";
        taskData["task"]["result"].clear();
        taskData["task"]["result"]["msg"] = "Unknown Error";
    }

    // 回送TaskData
    std::cout << "Push back task: " << taskID << endl;
    RabbitMQPush mqWorker;
    mqWorker.pushTaskData(taskData);

    cout << "Finish Task: " << taskID << endl;
}