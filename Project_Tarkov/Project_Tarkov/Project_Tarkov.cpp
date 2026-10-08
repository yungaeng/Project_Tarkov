#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "Core/Application.h"
#include "Core/Logger.h"
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace
{
    void SetAssetWorkingDirectory()
    {
        // Keep the source assets available for shader hot reload in Visual Studio.
        if (std::filesystem::is_directory("Assets")) return;

        std::vector<wchar_t> path(32768);
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size())
            throw std::runtime_error("Cannot determine the executable directory");

        const auto directory = std::filesystem::path(path.data()).parent_path();
        if (!std::filesystem::is_directory(directory / "Assets"))
            throw std::runtime_error("Assets folder is missing next to Project_Tarkov.exe. Rebuild the project.");
        std::filesystem::current_path(directory);
    }
}

int main()
{
    try
    {
        SetAssetWorkingDirectory();
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
