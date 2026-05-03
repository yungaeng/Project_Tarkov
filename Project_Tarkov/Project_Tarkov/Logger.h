#pragma once
#include "pch.h"

class Logger
{
public:
    static void Init()
    {
        std::cout << "[Logger] Init\n";
    }

    static void Info(const std::string& msg)
    {
        std::cout << "[INFO] " << msg << "\n";
    }

    static void Shutdown()
    {
        std::cout << "[Logger] Shutdown\n";
    }
};
