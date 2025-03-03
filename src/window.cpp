#include "window.h"
#include <iostream>

void Window::errorCallback(int error, const char *description)
{
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

void Window::framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

Window::Window(int width, int height, const std::string &title)
    : width(width), height(height), title(title), window(nullptr)
{
}

Window::~Window()
{
    if (window)
    {
        glfwDestroyWindow(window);
    }
    // Note: We don't call glfwTerminate() here because there might be multiple windows
    // Let the main function handle the GLFW termination
}

bool Window::initialize()
{
    // Set error callback
    glfwSetErrorCallback(errorCallback);

    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Create window
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    return true;
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(window);
}

void Window::swapBuffers()
{
    glfwSwapBuffers(window);
}

void Window::pollEvents()
{
    glfwPollEvents();
}

int Window::getWidth() const
{
    return width;
}

int Window::getHeight() const
{
    return height;
}

GLFWwindow *Window::getGLFWWindow() const
{
    return window;
}

bool Window::isKeyPressed(int key) const
{
    return glfwGetKey(window, key) == GLFW_PRESS;
}

void Window::getCursorPosition(double &x, double &y) const
{
    glfwGetCursorPos(window, &x, &y);
}

void Window::setCursorPosition(double x, double y)
{
    glfwSetCursorPos(window, x, y);
}

void Window::enableCursorCapture(bool enable)
{
    if (enable)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Window::getMouseDelta(double &dx, double &dy)
{
    double currentX, currentY;
    getCursorPosition(currentX, currentY);

    if (firstMouse)
    {
        // First mouse movement - no delta yet
        lastMouseX = currentX;
        lastMouseY = currentY;
        firstMouse = false;
        dx = 0.0;
        dy = 0.0;
        return;
    }

    // Calculate movement since last check
    dx = currentX - lastMouseX;
    dy = lastMouseY - currentY; // Reversed since y-coordinates go from bottom to top

    // Update last position
    lastMouseX = currentX;
    lastMouseY = currentY;
}