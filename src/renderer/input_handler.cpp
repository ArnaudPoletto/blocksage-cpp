#include "renderer/input_handler.h"

const float initialMoveSpeed = 5.0f;
const float initialMoveSpeedIncreaseFactor = 5.0f;
const float initialMouseSensitivity = 0.1f;

InputHandler::InputHandler(
    glm::vec3 &cameraPos,
    glm::vec3 &cameraFront,
    glm::vec3 &cameraRight,
    glm::vec3 &cameraUp,
    float &yaw,
    float &pitch,
    bool &isRunning,
    bool &developerModeActive,
    std::function<void()> updateCameraVectorsCallback)
    : cameraPos(cameraPos),
      cameraFront(cameraFront),
      cameraRight(cameraRight),
      cameraUp(cameraUp),
      yaw(yaw),
      pitch(pitch),
      isRunning(isRunning),
      developerModeActive(developerModeActive),
      updateCameraVectorsCallback(updateCameraVectorsCallback),
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
    processMouse(window, yaw, pitch);
    updateCameraVectorsCallback();
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
        double dx = cameraFront.x * distance;
        double dz = cameraFront.z * distance;
        cameraPos += glm::vec3(dx, 0.0f, dz);
    }
    if (window.isKeyPressed(GLFW_KEY_S))
    {
        double dx = cameraFront.x * distance;
        double dz = cameraFront.z * distance;
        cameraPos -= glm::vec3(dx, 0.0f, dz);
    }

    // Left/right movement
    if (window.isKeyPressed(GLFW_KEY_A))
    {
        double dx = cameraRight.x * distance;
        double dz = cameraRight.z * distance;
        cameraPos -= glm::vec3(dx, 0.0f, dz);
    }
    if (window.isKeyPressed(GLFW_KEY_D))
    {
        double dx = cameraRight.x * distance;
        double dz = cameraRight.z * distance;
        cameraPos += glm::vec3(dx, 0.0f, dz);
    }

    // Up/down movement
    if (window.isKeyPressed(GLFW_KEY_SPACE))
    {
        double dy = distance;
        cameraPos += glm::vec3(0.0f, dy, 0.0f);
    }
    if (window.isKeyPressed(GLFW_KEY_Q))
    {
        double dy = distance;
        cameraPos -= glm::vec3(0.0f, dy, 0.0f);
    }

    // Developer mode
    static bool developerKeyPressed = false;
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