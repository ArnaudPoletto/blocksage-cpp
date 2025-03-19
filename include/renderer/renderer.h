#pragma once

#include "opengl_headers.h"
#include "window.h"
#include "region.h"
#include "input_handler.h"
#include "shaders_setup.h"
#include <cmath>
#include <unordered_map>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <future>

#define PI 3.14159265359f

struct BlockFace {
    glm::vec3 position;
    uint8_t face; // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
};

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
    // Lighting
    GLint cubeViewPosLoc;
    glm::vec3 lightDirection;

    // Threading
    std::vector<std::thread> threads;
    std::queue<std::tuple<int, int, int, std::string>> sectionQueue;
    std::mutex queueMutex;
    std::mutex cacheMutex;
    std::condition_variable condition;
    std::atomic<bool> stopThreads;
    int maxNThreads;
    int nThreads;

    std::unordered_map<std::string, std::future<void>> pendingProcessing;
    std::mutex pendingMutex;

    void workerFunction();
    void queueSectionForProcessing(int sx, int sy, int sz, const std::string& sectionKey);
    bool isSectionReady(const std::string& sectionKey);

    // Section cache
    struct SectionCache {
        std::unordered_map<uint16_t, std::vector<BlockFace>> blockFaces;
        bool dirty;
        bool processing;

        SectionCache() : dirty(true), processing(false) {}
    };
    std::unordered_map<std::string, SectionCache> sectionCache;

    // Shaders
    ShadersSetup shadersSetup;
    GLuint axesVAO, axesVBO;
    int axesVertexCount;

    GLuint currentSectionBoundsVAO, currentSectionBoundsVBO;
    int currentSectionBoundsVertexCount;

    GLuint cubeVAO, cubeVBO, cubeEBO;
    GLuint instanceVBO;
    GLuint faceFlagsVBO;
    int cubeVertexCount;

    std::vector<unsigned int> cubeFaceIndices[6];

    GLuint compileShader(GLenum type, const char *source);
    GLuint createShaderProgram(const char *vertexShaderSource, const char *fragmentShaderSource);
    bool setupShaders();
    bool setupAxesGeometry();
    bool setupCurrentSectionBoundsGeometry();
    bool setupCubeGeometry();

    // Camera and movement
    glm::vec3 cameraPos;
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    glm::vec3 cameraRight;
    float yaw;
    float pitch;
    glm::ivec3 lastCameraSectionPos;

    void updateCameraVectors();

    // Drawing
    void drawAxes(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, float delta = 0.001f);
    void drawCurrentSectionBounds(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix);
    void processSection(int sx, int sy, int sz, const std::string& sectionKey);
    void renderSection(const std::string& sectionKey, const glm::mat4& viewProjectionMatrix);
    void drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance = 6);

    // Rendering
    bool isRunning;
    bool developerModeActive;
    float lastFrameTime;
    void renderFrame(int windowWidth, int windowHeight, float nearPlane = 0.1f, float farPlane = 5000.0f);

    // Input
    InputHandler inputHandler;
    void handleInput(Window &window, float deltaTime);

    // Data
    Region *region;
    std::unordered_map<uint16_t, glm::vec3> blockColorDict;
    std::vector<uint16_t> noRenderBlockIds;

    // Setters
    void setCameraPosition(float x, float y, float z);
    void setLookAtPoint(float x, float y, float z);

    // Getters
    std::string getSectionKey(int x, int y, int z);
};