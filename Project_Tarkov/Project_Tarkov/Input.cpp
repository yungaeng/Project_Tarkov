#include "pch.h"
#include "Input.h"

GLFWwindow* Input::window = nullptr;
bool Input::current[512];
bool Input::previous[512];