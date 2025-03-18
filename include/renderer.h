#pragma once

#include "opengl_headers.h"
#include "window.h"
#include "region.h"
#include <cmath>
#include <unordered_map>
#include <vector>

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
    GLuint baseShaderProgram;
    GLuint axesVAO, axesVBO;
    int axesVertexCount;

    GLuint currentSectionBoundsVAO, currentSectionBoundsVBO;
    int currentSectionBoundsVertexCount;

    GLuint cubeShaderProgram;
    GLuint cubeVAO, cubeVBO, cubeEBO;
    GLuint instanceVBO;
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
    glm::ivec3 lastCameraSectionPos;

    // Rendering
    bool isRunning;
    bool developerModeActive;
    float deltaTime;
    float lastFrameTime;

    // Uniform location cache
    GLint cubeVPMatrixLoc;
    GLint cubeColorOverrideLoc;
    GLint cubeUseColorOverrideLoc;
    GLint axesMVPMatrixLoc;

    // Lighting
    GLint cubeLightDirLoc;
    GLint cubeViewPosLoc;
    glm::vec3 lightDirection;

    // Data
    Region *region;
    std::unordered_map<uint16_t, glm::vec3> blockColorDict;
    std::vector<uint16_t> noRenderBlockIds;

    // Section cache
    struct SectionCache {
        std::unordered_map<uint16_t, std::vector<glm::vec3>> blockGroups;
        bool dirty;
    };
    std::unordered_map<std::string, SectionCache> sectionCache;

    // Shaders
    GLuint compileShader(GLenum type, const char *source);
    GLuint createShaderProgram(const char *vertexShaderSource, const char *fragmentShaderSource);
    bool setupShaders();
    bool setupAxesGeometry();
    bool setupCurrentSectionBoundsGeometry();
    bool setupCubeGeometry();

    // Drawing
    void drawAxes(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, float delta = 0.001f);
    void drawCurrentSectionBounds(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix);
    void processSection(int sx, int sy, int sz, const std::string& sectionKey);
    void renderSection(const std::string& sectionKey, const glm::mat4& viewProjectionMatrix);
    void drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance = 32);

    // Rendering
    void renderFrame(int windowWidth, int windowHeight, float nearPlane = 0.1f, float farPlane = 5000.0f);

    // Camera
    void updateCameraVectors();

    // Input
    void handleInput(Window &window);
    void processMouse(Window &window);

    // Setters
    void setCameraPosition(float x, float y, float z);
    void setLookAtPoint(float x, float y, float z);

    // Getters
    std::string getSectionKey(int x, int y, int z) const {
        return std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z);
    }
};