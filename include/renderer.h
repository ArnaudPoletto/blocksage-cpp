#pragma once

#include <windows.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <cmath>

#define PI 3.14159265359f

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void renderFrame(int windowWidth, int windowHeight);
    void startRenderLoop(class Window &window);

    void setCameraPosition(float x, float y, float z);
    void setLookAtPoint(float x, float y, float z);

    void setCircleProperties(float radius, int segments);

private:
    void setupCamera(int windowWidth, int windowHeight);
    void moveCamera(float dx, float dy, float dz);
    void processMouse(Window &window);
    void updateCameraVectors();

    void handleInput(Window &window);

    void drawCircle();

    float cameraX, cameraY, cameraZ;
    float lookX, lookY, lookZ;

    float forwardX, forwardY, forwardZ;
    float rightX, rightY, rightZ;
    float upX, upY, upZ;
    float yaw;   // Rotation around Y axis (left/right)
    float pitch; // Rotation around X axis (up/down)

    float moveSpeed;
    float moveSpeedIncreaseFactor;
    float mouseSensitivity;

    float circleRadius;
    int circleSegments;

    bool isRunning;

    float deltaTime;
    float lastFrameTime;
};