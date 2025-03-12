#pragma once

#include "opengl_headers.h"
#include <string>
#include <cmath>

class Window
{
public:
    Window(int width, int height, const std::string &title);
    ~Window();

    bool initialize();
    void cleanup();

    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();

    int getWidth() const;
    int getHeight() const;
    GLFWwindow *getGLFWWindow() const;

    bool isKeyPressed(int key) const;
    void getCursorPosition(double &x, double &y) const;
    void setCursorPosition(double x, double y);
    void enableCursorCapture(bool enable);
    void getMouseDelta(double &dx, double &dy);

private:
    int width;
    int height;
    std::string title;
    GLFWwindow *window;

    double lastMouseX;
    double lastMouseY;
    bool firstMouse;

    static void errorCallback(int error, const char *description);
    static void framebufferSizeCallback(GLFWwindow *window, int width, int height);
};