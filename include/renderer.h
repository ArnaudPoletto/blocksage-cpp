#pragma once

#include "opengl_headers.h"
#include "window.h"
#include "region.h"
#include <cmath>
#include <unordered_map>

#define PI 3.14159265359f

class Renderer
{
public:
    // Constructor and destructor
    Renderer(const std::unordered_map<uint16_t, glm::vec3> &blockColorDict, std::vector<uint16_t> noRenderBlockIds);
    ~Renderer();
    bool initialize();

    // Rendering
    void startRenderLoop(Window &window);

    // Setters
    void setRegion(Region *region);

private:
    // Shaders
    GLuint axesShaderProgram;
    GLuint axesVAO, axesVBO;
    int axesVertexCount;

    GLuint cubeShaderProgram;
    GLuint cubeVAO, cubeVBO, cubeEBO;
    GLuint instanceVBO; // Stores position of each cube 
    int cubeVertexCount;

    // Camera and movement
    glm::vec3 cameraPos;
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    glm::vec3 cameraRight;
    float yaw, pitch;
    float moveSpeed;
    float moveSpeedIncreaseFactor;
    float mouseSensitivity;

    // Rendering
    bool isRunning;
    float deltaTime;
    float lastFrameTime;

    // Data
    Region *region;
    std::unordered_map<uint16_t, glm::vec3> blockColorDict;
    std::vector<uint16_t> noRenderBlockIds;

    // Shaders
    GLuint compileShader(GLenum type, const char *source);
    GLuint createShaderProgram(const char *vertexShaderSource, const char *fragmentShaderSource);
    bool setupShaders();
    bool setupAxesGeometry();
    bool setupCubeGeometry();

    // Drawing
    void drawAxes(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, float delta = 0.001f);
    void drawCube(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, glm::vec3 position, glm::vec3 color);
    void drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance);

    // Rendering
    void renderFrame(int windowWidth, int windowHeight);

    // Camera
    void updateCameraVectors();

    // Input
    void handleInput(Window &window);
    void processMouse(Window &window);

    // Setters
    void setCameraPosition(float x, float y, float z);
    void setLookAtPoint(float x, float y, float z);
};