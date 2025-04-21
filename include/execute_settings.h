#pragma once

#define MQ_HOST "192.168.31.6"
#define MQ_PORT 5672
#define MQ_USER "qingniao"
#define MQ_PASSWORD "123456"
#define FILE_ROOT_PATH "/tmp/judge/"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

enum class TaskStatus
{
    COMPILING = 2,
    EXECUTING = 1,
    AC = 0,
    WA = -1,  //
    UKE = -2, //
    RE = -3,  //
    CE = -4,
    TLE = -5, //
    MLE = -6, //
    OLE = -7  //
};

// 自定义需要直接中断执行的错误类型对应的异常
// TLE
class time_limit_error : public std::exception
{
private:
    std::string msg;

public:
    time_limit_error(const std::string &msg) : msg(msg) {}

    const char *what() const noexcept override
    {
        return msg.c_str();
    }
};

// MLE
class memory_limit_error : public std::exception
{
private:
    std::string msg;

public:
    memory_limit_error(const std::string &msg) : msg(msg) {}

    const char *what() const noexcept override
    {
        return msg.c_str();
    }
};