#pragma once
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_LAST 348
#define GLFW_TRUE 1
#define GLFW_PRESS 1
#define GLFW_FOCUSED 0x20001
#define GLFW_CURSOR 0x33001
#define GLFW_CURSOR_NORMAL 0x34001
#define GLFW_CURSOR_DISABLED 0x34003
struct GLFWwindow;
using GLFWwindowfocusfun = void (*)(GLFWwindow*, int);
struct GLFWwindow
{
    bool focused = true;
    int cursor = GLFW_CURSOR_DISABLED;
    int keys[GLFW_KEY_LAST + 1] = {};
    double x = 0, y = 0;
    GLFWwindowfocusfun focusCallback = nullptr;
};
inline int glfwGetWindowAttrib(GLFWwindow* w, int) { return w->focused; }
inline int glfwGetKey(GLFWwindow* w, int key) { return w->keys[key]; }
inline int glfwGetInputMode(GLFWwindow* w, int) { return w->cursor; }
inline void glfwSetInputMode(GLFWwindow* w, int, int mode) { w->cursor = mode; }
inline void glfwGetCursorPos(GLFWwindow* w, double* x, double* y)
{
    *x = w->x;
    *y = w->y;
}
inline GLFWwindowfocusfun glfwSetWindowFocusCallback(GLFWwindow* w, GLFWwindowfocusfun callback)
{
    auto previous = w->focusCallback;
    w->focusCallback = callback;
    return previous;
}
