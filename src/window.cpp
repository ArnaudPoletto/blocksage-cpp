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
    : width(width), height(height), title(title), window(nullptr), firstMouse(true),
      lastMouseX(0.0), lastMouseY(0.0)
{
}

Window::~Window()
{
    // Only destroy the window if GLFW is still initialized
    if (window && glfwGetCurrentContext() != nullptr)
    {
        glfwDestroyWindow(window);
    }
    window = nullptr;
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

    // Configure GLFW for OpenGL 4.6
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        glfwDestroyWindow(window);
        return false;
    }

    // Print OpenGL version
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Set OpenGL options
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    return true;
}

void Window::cleanup()
{
    if (window)
    {
        glfwDestroyWindow(window);
        window = nullptr;
    }
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