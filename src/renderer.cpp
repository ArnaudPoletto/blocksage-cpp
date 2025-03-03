#include "renderer.h"
#include "window.h"
#include <math.h>
#include <iostream>

Renderer::Renderer()
{
    cameraX = 1.0f;
    cameraY = 1.0f;
    cameraZ = 1.0f;

    lookX = 0.0f;
    lookY = 0.0f;
    lookZ = 0.0f;

    // Get normalized forward vector
    forwardX = lookX - cameraX;
    forwardY = lookY - cameraY;
    forwardZ = lookZ - cameraZ;
    float forwardLength = sqrtf(forwardX * forwardX + forwardY * forwardY + forwardZ * forwardZ);
    if (forwardLength > 0.0f)
    {
        forwardX /= forwardLength;
        forwardY /= forwardLength;
        forwardZ /= forwardLength;
    }

    // Get normalized right vector
    rightX = forwardZ; // Cross product with (0,1,0)
    rightY = 0.0f;
    rightZ = -forwardX;
    float rightLength = sqrtf(rightX * rightX + rightY * rightY + rightZ * rightZ);
    if (rightLength > 0.0f)
    {
        rightX /= rightLength;
        rightY /= rightLength;
        rightZ /= rightLength;
    }

    // Get up vector
    upX = rightY * forwardZ - rightZ * forwardY;
    upY = rightZ * forwardX - rightX * forwardZ;
    upZ = rightX * forwardY - rightY * forwardX;

    yaw = -0.0f * (PI / 180.0f);
    pitch = -0.0f * (PI / 180.0f);

    moveSpeed = 5.0f;
    moveSpeedIncreaseFactor = 2.0f;
    mouseSensitivity = 0.003f;

    circleRadius = 0.5f;
    circleSegments = 32;

    isRunning = true;

    deltaTime = 0.0f;
    lastFrameTime = 0.0f;

    updateCameraVectors();
}

Renderer::~Renderer()
{
}

void Renderer::renderFrame(int windowWidth, int windowHeight)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCamera(windowWidth, windowHeight);
    drawCircle();
}

void Renderer::startRenderLoop(Window &window)
{
    window.enableCursorCapture(true);

    while (!window.shouldClose() && isRunning)
    {
        handleInput(window);
        renderFrame(window.getWidth(), window.getHeight());
        window.swapBuffers();
        window.pollEvents();
    }

    window.enableCursorCapture(false);
}

void Renderer::setCameraPosition(float x, float y, float z)
{
    cameraX = x;
    cameraY = y;
    cameraZ = z;
}

void Renderer::setLookAtPoint(float x, float y, float z)
{
    lookX = x;
    lookY = y;
    lookZ = z;
}

void Renderer::setCircleProperties(float radius, int segments)
{
    circleRadius = radius;
    circleSegments = segments;
}

void Renderer::setupCamera(int windowWidth, int windowHeight)
{
    // Set up a perspective projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)windowWidth / (float)windowHeight, 0.1f, 100.0f);

    // Set up the camera position (view matrix)
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        cameraX, cameraY, cameraZ, // Camera position
        lookX, lookY, lookZ,       // Look at point
        0.0f, 1.0f, 0.0f           // Up vector
    );
}

void Renderer::moveCamera(float dx, float dy, float dz)
{
    // Update camera position
    cameraX += dx;
    cameraY += dy;
    cameraZ += dz;

    // Update look at point based on camera position and forward vector
    lookX = cameraX + forwardX;
    lookY = cameraY + forwardY;
    lookZ = cameraZ + forwardZ;
}

void Renderer::processMouse(Window &window)
{
    double dx, dy;
    window.getMouseDelta(dx, dy);

    // Apply sensitivity
    dx *= mouseSensitivity;
    dy *= mouseSensitivity;

    // Update camera orientation
    yaw += dx;
    pitch += dy;

    // Constrain pitch to avoid flipping
    if (pitch > PI / 2.0f - 0.01f)
    {
        pitch = PI / 2.0f - 0.01f;
    }
    if (pitch < -PI / 2.0f + 0.01f)
    {
        pitch = -PI / 2.0f + 0.01f;
    }

    // Update camera vectors based on new orientation
    updateCameraVectors();
}

void Renderer::updateCameraVectors()
{
    // Calculate front vector from yaw and pitch angles
    forwardX = cosf(yaw) * cosf(pitch);
    forwardY = sinf(pitch);
    forwardZ = sinf(yaw) * cosf(pitch);

    // Normalize forward vector
    float forwardLength = sqrtf(forwardX * forwardX + forwardY * forwardY + forwardZ * forwardZ);
    if (forwardLength > 0.0f)
    {
        forwardX /= forwardLength;
        forwardY /= forwardLength;
        forwardZ /= forwardLength;
    }

    // Calculate right vector (cross product of forward and world up)
    rightX = forwardZ; // Cross product with (0,1,0)
    rightY = 0.0f;
    rightZ = -forwardX;

    // Normalize right vector
    float rightLength = sqrtf(rightX * rightX + rightY * rightY + rightZ * rightZ);
    if (rightLength > 0.0f)
    {
        rightX /= rightLength;
        rightY /= rightLength;
        rightZ /= rightLength;
    }

    // Calculate up vector (cross product of right and forward)
    upX = rightY * forwardZ - rightZ * forwardY;
    upY = rightZ * forwardX - rightX * forwardZ;
    upZ = rightX * forwardY - rightY * forwardX;

    // Update look at point based on camera position and forward vector
    lookX = cameraX + forwardX;
    lookY = cameraY + forwardY;
    lookZ = cameraZ + forwardZ;
}

void Renderer::handleInput(Window &window)
{
    // Calculate delta time for smooth movement
    float currentTime = (float)glfwGetTime();
    deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    // Calculate actual movement distance based on time
    float distance = moveSpeed * deltaTime;
    if (window.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
    {
        distance *= moveSpeedIncreaseFactor;
    }

    if (window.isKeyPressed(GLFW_KEY_W))
    {
        moveCamera(forwardX * distance, 0.0f, forwardZ * distance);
    }
    if (window.isKeyPressed(GLFW_KEY_S))
    {
        moveCamera(-forwardX * distance, 0.0f, -forwardZ * distance);
    }
    if (window.isKeyPressed(GLFW_KEY_A))
    {
        moveCamera(rightX * distance, 0.0f, rightZ * distance);
    }
    if (window.isKeyPressed(GLFW_KEY_D))
    {
        moveCamera(-rightX * distance, 0.0f, -rightZ * distance);
    }

    if (window.isKeyPressed(GLFW_KEY_SPACE))
    {
        moveCamera(0.0f, distance, 0.0f);
    }
    if (window.isKeyPressed(GLFW_KEY_Q))
    {
        moveCamera(0.0f, -distance, 0.0f);
    }

    if (window.isKeyPressed(GLFW_KEY_ESCAPE))
    {
        isRunning = false;
    }

    processMouse(window);
}

void Renderer::drawCircle()
{
    glBegin(GL_TRIANGLE_FAN);

    // Center of circle
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);

    // Draw the circle segments
    for (int i = 0; i <= circleSegments; i++)
    {
        float angle = 2.0f * PI * i / circleSegments;
        float x = circleRadius * cosf(angle);
        float y = circleRadius * sinf(angle);
        glColor3f(1.0f, 0.0f, 0.0f); // Red outline
        glVertex3f(x, y, 0.0f);
    }

    glEnd();
}
