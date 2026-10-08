#pragma once
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

class Input
{
private:
    inline static GLFWwindow* window = nullptr;
    inline static bool current[GLFW_KEY_LAST + 1] = {};
    inline static bool previous[GLFW_KEY_LAST + 1] = {};
    inline static bool focused = false;
    inline static bool buttons[GLFW_MOUSE_BUTTON_LAST + 1] = {};
    inline static bool oldButtons[GLFW_MOUSE_BUTTON_LAST + 1] = {};
    inline static unsigned focusGeneration = 0;
    inline static bool mouseReady = false;
    inline static double lastX = 0, lastY = 0;
    inline static glm::vec2 mouseDelta = { 0, 0 };

    static bool ValidKey(int key)
    {
        return key >= GLFW_KEY_SPACE && key <= GLFW_KEY_LAST;
    }

public:
    static void ResetMouse()
    {
        mouseReady = false;
        mouseDelta = { 0, 0 };
    }
    static void Init(GLFWwindow* w)
    {
        Shutdown();
        window = w;
        if (window)
            glfwSetWindowFocusCallback(window, [](GLFWwindow*, int)
            {
                // Also handle a loss and regain occurring within one event poll.
                focused = false;
                ++focusGeneration;
                ResetMouse();
            });
    }
    static void Shutdown()
    {
        if (window) glfwSetWindowFocusCallback(window, nullptr);
        window = nullptr;
        focused = false;
        for (int i = 0; i <= GLFW_KEY_LAST; ++i)
            current[i] = previous[i] = false;
        for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; ++i) buttons[i] = oldButtons[i] = false;
        ResetMouse();
    }
    static void SetCursorCaptured(bool captured)
    {
        if (window)
            glfwSetInputMode(window, GLFW_CURSOR,
                captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        ResetMouse();
    }
    static void Update()
    {
        const bool nowFocused = window &&
            glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
        const bool regainedFocus = nowFocused && !focused;
        focused = nowFocused;
        for (int i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; ++i)
        {
            previous[i] = current[i];
            current[i] = focused && glfwGetKey(window, i) == GLFW_PRESS;
            // Held keys on focus restoration must not trigger toggles.
            if (regainedFocus) previous[i] = current[i];
        }
        for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; ++i)
        {
            oldButtons[i] = buttons[i];
            buttons[i] = focused && glfwGetMouseButton(window, i) == GLFW_PRESS;
            if (regainedFocus) oldButtons[i] = buttons[i];
        }
        mouseDelta = { 0, 0 };
        if (!focused || glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
        {
            ResetMouse();
            return;
        }
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        if (mouseReady && !regainedFocus)
            mouseDelta = { static_cast<float>(x - lastX), static_cast<float>(lastY - y) };
        lastX = x;
        lastY = y;
        mouseReady = true;
    }
    static unsigned FocusGeneration() { return focusGeneration; }
    static bool Mouse(int button) { return button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST && buttons[button]; }
    static bool MouseDown(int button) { return Mouse(button) && !oldButtons[button]; }
    static bool IsFocused() { return focused; }
    static glm::vec2 GetMouseDelta() { return mouseDelta; }
    static bool GetKey(int key) { return ValidKey(key) && current[key]; }
    static bool GetKeyDown(int key)
    {
        return ValidKey(key) && current[key] && !previous[key];
    }
    static bool GetKeyUp(int key)
    {
        return ValidKey(key) && !current[key] && previous[key];
    }
};
