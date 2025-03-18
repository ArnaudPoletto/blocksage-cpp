#pragma once

#include "window.h"
#include <iostream>
#include <functional>

class InputHandler
{
public:
    InputHandler(
        glm::vec3 &cameraPos,
        glm::vec3 &cameraFront,
        glm::vec3 &cameraRight,
        glm::vec3 &cameraUp,
        float &yaw,
        float &pitch,
        bool &isRunning,
        bool &developerModeActive,
        std::function<void()> updateCameraVectorsCallback);
    ~InputHandler();

    void handleInput(Window &window, float deltaTime);

private:
    // Input data
    float moveSpeed;
    float moveSpeedIncreaseFactor;
    float mouseSensitivity;
    bool developerKeyPressed;

    // Parent data
    glm::vec3 &cameraPos;
    glm::vec3 &cameraFront;
    glm::vec3 &cameraRight;
    glm::vec3 &cameraUp;
    float &yaw;
    float &pitch;
    bool &isRunning;
    bool &developerModeActive;
    std::function<void()> updateCameraVectorsCallback;

    void processKeyboard(Window &window, float deltaTime);
    void processMouse(Window &window, float &yaw, float &pitch);
};