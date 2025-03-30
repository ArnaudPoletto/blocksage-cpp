#pragma once

#include "window.h"
#include "camera.h"
#include <iostream>
#include <functional>

class InputHandler
{
public:
    InputHandler(
        Camera &camera,
        bool &isRunning,
        bool &developerModeActive);
    ~InputHandler();

    void handleInput(Window &window, float deltaTime);

private:
    // Input data
    float moveSpeed;
    float moveSpeedIncreaseFactor;
    float mouseSensitivity;
    bool developerKeyPressed;

    // Parent data
    Camera &camera;
    bool &isRunning;
    bool &developerModeActive;

    void processKeyboard(Window &window, float deltaTime);
    void processMouse(Window &window, float &yaw, float &pitch);
};