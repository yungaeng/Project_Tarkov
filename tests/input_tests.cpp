// The test include path supplies a deterministic GLFW backend; production Input is unchanged.
#include "../Project_Tarkov/Project_Tarkov/Input.h"
#include <cassert>
#include <iostream>

int main()
{
    Input::Update(); // Safe before initialization.
    assert(!Input::GetKey(-1));
    assert(!Input::GetKeyDown(GLFW_KEY_LAST + 1));
    assert(!Input::GetKeyUp(-100));

    GLFWwindow window;
    Input::Init(&window);
    Input::Update();
    window.keys[73] = GLFW_PRESS;
    Input::Update();
    assert(Input::GetKeyDown(73));
    Input::Update();
    assert(Input::GetKey(73) && !Input::GetKeyDown(73));
    window.keys[73] = 0;
    Input::Update();
    assert(Input::GetKeyUp(73));

    window.x = 10;
    window.y = 5;
    Input::Update();
    assert(Input::GetMouseDelta().x == 10 && Input::GetMouseDelta().y == -5);

    Input::SetCursorCaptured(false);
    window.x = 1000;
    Input::Update();
    assert(Input::GetMouseDelta().x == 0);
    Input::SetCursorCaptured(true);
    assert(Input::GetMouseDelta().x == 0);
    Input::Update();
    assert(Input::GetMouseDelta().x == 0);
    window.x = 1002;
    Input::Update();
    assert(Input::GetMouseDelta().x == 2);

    window.keys[73] = GLFW_PRESS;
    window.focused = false;
    window.focusCallback(&window, 0);
    Input::Update();
    assert(!Input::IsFocused() && !Input::GetKey(73));
    window.focused = true;
    window.x = 2000;
    window.focusCallback(&window, 1);
    Input::Update();
    assert(Input::GetKey(73) && !Input::GetKeyDown(73));
    assert(Input::GetMouseDelta().x == 0);

    window.focusCallback(&window, 0);
    window.focusCallback(&window, 1);
    window.x = 3000;
    Input::Update();
    assert(Input::GetMouseDelta().x == 0 && !Input::GetKeyDown(73));

    Input::ResetMouse(); // Fullscreen transition.
    window.x = 4000;
    Input::Update();
    assert(Input::GetMouseDelta().x == 0);

    Input::Shutdown();
    assert(window.focusCallback == nullptr);
    Input::Update();
    assert(!Input::IsFocused() && !Input::GetKey(73));
    Input::Init(&window);
    Input::Update();
    assert(!Input::GetKeyDown(73) && Input::GetMouseDelta().x == 0);
    Input::Shutdown();
    std::cout << "Input regression tests passed\n";
}
