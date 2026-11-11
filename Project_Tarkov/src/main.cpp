#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "Core/Application.h"
#include "Core/RuntimePaths.h"
#include "Core/Logger.h"
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <vector>

int main()
{
    try
    {
        RuntimePaths::Initialize();
        Application app;
        if (!app.Init()) return -1;
        app.Run();
    }
    catch (const std::exception& error)
    {
        Logger::Error(error.what());
        return -1;
    }
    return 0;
}
