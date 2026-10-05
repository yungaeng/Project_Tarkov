#include "Application.h"
#include <exception>

int main()
{
    try
    {
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
