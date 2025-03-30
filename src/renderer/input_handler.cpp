#include "renderer/input_handler.h"

const float initialMoveSpeed = 5.0f;
const float initialMoveSpeedIncreaseFactor = 5.0f;
const float initialMouseSensitivity = 0.1f;

InputHandler::InputHandler(
    Camera &camera,
    bool &isRunning,
    bool &developerModeActive)
    : camera(camera),
      isRunning(isRunning),
      developerModeActive(developerModeActive),
      moveSpeed(initialMoveSpeed),
      moveSpeedIncreaseFactor(initialMoveSpeedIncreaseFactor),
      mouseSensitivity(initialMouseSensitivity),
      developerKeyPressed(false)
{
}

InputHandler::~InputHandler() {}

void InputHandler::handleInput(Window &window, float deltaTime)
{
    processKeyboard(window, deltaTime);
    processMouse(window, camera.yaw, camera.pitch);
    camera.updateCameraVectors();
}

void InputHandler::processKeyboard(Window &window, float deltaTime)
{
    // Speed increase
    float speed = moveSpeed;
    if (window.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
    {
        speed *= moveSpeedIncreaseFactor;
    }
    float distance = speed * deltaTime;

    // Forward/backward movement
    if (window.isKeyPressed(GLFW_KEY_W))
    {
        camera.position += camera.front * distance * glm::vec3(1.0f, 0.0f, 1.0f);
    }
    if (window.isKeyPressed(GLFW_KEY_S))
    {
        camera.position -= camera.front * distance * glm::vec3(1.0f, 0.0f, 1.0f);
    }

    // Left/right movement
    if (window.isKeyPressed(GLFW_KEY_A))
    {
        camera.position -= camera.right * distance * glm::vec3(1.0f, 0.0f, 1.0f);
    }
    if (window.isKeyPressed(GLFW_KEY_D))
    {
        camera.position += camera.right * distance * glm::vec3(1.0f, 0.0f, 1.0f);
    }

    // Up/down movement
    if (window.isKeyPressed(GLFW_KEY_SPACE))
    {
        double dy = distance;
        camera.position += glm::vec3(0.0f, dy, 0.0f);
    }
    if (window.isKeyPressed(GLFW_KEY_Q))
    {
        double dy = distance;
        camera.position -= glm::vec3(0.0f, dy, 0.0f);
    }

    // Developer mode
    if (window.isKeyPressed(GLFW_KEY_G))
    {
        if (!developerKeyPressed)
        {
            developerKeyPressed = true;
            developerModeActive = !developerModeActive;
            std::cout << "InputHandler: Developer mode: " << (developerModeActive ? "enabled" : "disabled") << std::endl;
        }
    }
    else
    {
        developerKeyPressed = false;
    }

    // Exit
    if (window.isKeyPressed(GLFW_KEY_ESCAPE))
    {
        isRunning = false;
        std::cout << "InputHandler: Exiting..." << std::endl;
    }
}

void InputHandler::processMouse(Window &window, float &yaw, float &pitch)
{
    double dx, dy;
    window.getMouseDelta(dx, dy);
    dx *= mouseSensitivity;
    dy *= mouseSensitivity;

    yaw += dx;
    pitch += dy;

    // Constrain pitch to avoid gimbal lock
    if (pitch > 89.0f)
    {
        pitch = 89.0f;
    }
    if (pitch < -89.0f)
    {
        pitch = -89.0f;
    }
}