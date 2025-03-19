#include "renderer/renderer.h"
#include "window.h"
#include "region.h"
#include "config.h"
#include "renderer/shader_sources.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cmath>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*****
 ****
 *** Constructor and destructor
 ****
 ******/

const glm::vec3 initialCameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
const glm::vec3 initialCameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
const glm::vec3 initialCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
const float initialYaw = -90.0f;
const float initialPitch = 0.0f;
const float initialDeveloperModeActive = false;
const glm::vec3 initialLightDirection = glm::vec3(0.2f, 1.0f, 0.7f);
const int maxNThreads = 8;

Renderer::Renderer(const std::unordered_map<uint16_t, glm::vec3> &blockColorDict, std::vector<uint16_t> noRenderBlockIds)
    : cameraPos(initialCameraPos),
      cameraFront(initialCameraFront),
      cameraUp(initialCameraUp),
      yaw(initialYaw),
      pitch(initialPitch),
      developerModeActive(initialDeveloperModeActive),
      lightDirection(glm::normalize(initialLightDirection)),
      blockColorDict(blockColorDict),
      noRenderBlockIds(noRenderBlockIds),
      stopThreads(false),
      isRunning(true),
      lastFrameTime(0.0f),
      region(nullptr),
      inputHandler(cameraPos, cameraFront, cameraRight, cameraUp, yaw, pitch, isRunning, developerModeActive, [this]()
                   { this->updateCameraVectors(); }),
      shadersSetup()
{

    updateCameraVectors();

    nThreads = std::thread::hardware_concurrency();
    if (maxNThreads > 0)
    {
        nThreads = std::min(nThreads, maxNThreads);
    }
    nThreads = std::max(nThreads, 1);

    threads.resize(nThreads);
    for (int i = 0; i < nThreads; ++i)
    {
        threads[i] = std::thread(&Renderer::workerFunction, this);
    }

    std::cout << "Started " << nThreads << " worker threads for section processing" << std::endl;
}

Renderer::~Renderer()
{
    // Signal threads to stop
    stopThreads = true;
    condition.notify_all();

    // Wait for threads to finish
    for (auto &thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    inputHandler.~InputHandler();
    shadersSetup.~ShadersSetup();

    // Clean up axes
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);

    // Clean up cube
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
}

bool Renderer::initialize()
{
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);

    if (!shadersSetup.initialize())
    {
        std::cerr << "Failed to set up shaders" << std::endl;
        return false;
    }

    if (!setupAxesGeometry())
    {
        std::cerr << "Failed to set up axes geometry" << std::endl;
        return false;
    }

    if (!setupCurrentSectionBoundsGeometry())
    {
        std::cerr << "Failed to set up current section bounds geometry" << std::endl;
        return false;
    }

    if (!setupCubeGeometry())
    {
        std::cerr << "Failed to set up cube geometry" << std::endl;
        return false;
    }

    return true;
}

/*****
 ****
 *** Geometry
 ****
 ******/

bool Renderer::setupAxesGeometry()
{
    std::vector<float> vertices = {
        // X axis (red)
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Origin
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // +X
        // Y axis (green)
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Origin
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // +Y
        // Z axis (blue)
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Origin
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f  // +Z
    };

    axesVertexCount = vertices.size() / 6;

    // Create VAO and VBO
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);

    // Bind VAO
    glBindVertexArray(axesVAO);

    // Bind VBO and upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in setupAxesGeometry: " << err << std::endl;
        return false;
    }

    return true;
}

bool Renderer::setupCurrentSectionBoundsGeometry()
{
    std::vector<float> vertices = {
        // Bottom face
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Bottom-back-left
        16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // Bottom-back-right

        16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Bottom-back-right
        16.0f, 0.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Bottom-front-right

        16.0f, 0.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Bottom-front-right
        0.0f, 0.0f, 16.0f, 1.0f, 1.0f, 1.0f,  // Bottom-front-left

        0.0f, 0.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Bottom-front-left
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Bottom-back-left

        // Top face
        0.0f, 16.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Top-back-left
        16.0f, 16.0f, 0.0f, 1.0f, 1.0f, 1.0f, // Top-back-right

        16.0f, 16.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Top-back-right
        16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Top-front-right

        16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Top-front-right
        0.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f,  // Top-front-left

        0.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Top-front-left
        0.0f, 16.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Top-back-left

        // Vertical edges
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Bottom-back-left
        0.0f, 16.0f, 0.0f, 1.0f, 1.0f, 1.0f, // Top-back-left

        16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Bottom-back-right
        16.0f, 16.0f, 0.0f, 1.0f, 1.0f, 1.0f, // Top-back-right

        16.0f, 0.0f, 16.0f, 1.0f, 1.0f, 1.0f,  // Bottom-front-right
        16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Top-front-right

        0.0f, 0.0f, 16.0f, 1.0f, 1.0f, 1.0f, // Bottom-front-left
        0.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f // Top-front-left
    };

    currentSectionBoundsVertexCount = vertices.size() / 6;

    // Create VAO and VBO
    glGenVertexArrays(1, &currentSectionBoundsVAO);
    glGenBuffers(1, &currentSectionBoundsVBO);

    // Bind VAO
    glBindVertexArray(currentSectionBoundsVAO);

    // Bind VBO and upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, currentSectionBoundsVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in setupCurrentSectionBoundsGeometry: " << err << std::endl;
        return false;
    }

    return true;
}

bool Renderer::setupCubeGeometry()
{
    std::vector<float> vertices = {
        // Position          // Color           // Normal
        // Front face (Z+) - Face 4
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-left
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-right
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-left

        // Back face (Z-) - Face 5
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Bottom-left
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Bottom-right
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Top-right
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Top-left

        // Left face (X-) - Face 1
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Bottom-back
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Bottom-front
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Top-front
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Top-back

        // Right face (X+) - Face 0
        1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Bottom-front
        1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Bottom-back
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Top-back
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Top-front

        // Bottom face (Y-) - Face 3
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Back-left
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Back-right
        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Front-right
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Front-left

        // Top face (Y+) - Face 2
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // Front-left
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // Front-right
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // Back-right
        0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f  // Back-left
    };

    // Define indices for each face separately
    std::vector<unsigned int> faceIndices[6] = {
        // Face 0: +X (Right)
        {12, 13, 14, 14, 15, 12},

        // Face 1: -X (Left)
        {8, 9, 10, 10, 11, 8},

        // Face 2: +Y (Top)
        {20, 21, 22, 22, 23, 20},

        // Face 3: -Y (Bottom)
        {16, 17, 18, 18, 19, 16},

        // Face 4: +Z (Front)
        {0, 1, 2, 2, 3, 0},

        // Face 5: -Z (Back)
        {4, 5, 6, 6, 7, 4}};

    // Store the indices for each face in the class
    for (int i = 0; i < 6; i++)
    {
        cubeFaceIndices[i] = faceIndices[i];
    }

    // Combined indices for drawing the full cube
    std::vector<unsigned int> indices;
    for (int i = 0; i < 6; i++)
    {
        indices.insert(indices.end(), faceIndices[i].begin(), faceIndices[i].end());
    }

    cubeVertexCount = indices.size();

    // Create VAO, VBO, and EBO
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glGenBuffers(1, &instanceVBO);
    glGenBuffers(1, &faceFlagsVBO);

    // Bind VAO
    glBindVertexArray(cubeVAO);

    // Bind VBO and upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Set up vertex attributes from VBO
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // Bind EBO and upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Bind instance VBO
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 1000 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW); // TODO: Set size dynamically

    // Set up instance position attributes
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // Update per instance

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in setupCubeGeometry: " << err << std::endl;
        return false;
    }

    return true;
}

/*****
 ****
 *** Drawing
 ****
 ******/

void Renderer::drawAxes(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, float delta)
{
    glUseProgram(shadersSetup.baseShaderProgram);

    // Create model matrix
    glm::vec3 dPosition = glm::vec3(delta, delta, delta);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, -dPosition);

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    glUniformMatrix4fv(shadersSetup.axesMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

    // Enable line width (if supported by the hardware)
    GLfloat originalLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);
    glLineWidth(3.0f);

    // Draw axes as lines
    glBindVertexArray(axesVAO);
    glDrawArrays(GL_LINES, 0, axesVertexCount);

    // Restore original line width
    glLineWidth(originalLineWidth);

    // Reset state
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::drawCurrentSectionBounds(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix)
{
    if (!developerModeActive)
    {
        return;
    }

    glUseProgram(shadersSetup.baseShaderProgram);

    // Calculate current section coordinates
    float sx = floor(cameraPos.x / 16) * 16;
    float sy = floor(cameraPos.y / 16) * 16;
    float sz = floor(cameraPos.z / 16) * 16;

    // Create model matrix to position the section bounds
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(sx, sy, sz));

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    glUniformMatrix4fv(shadersSetup.axesMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

    // Enable line width for better visibility
    GLfloat originalLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);
    glLineWidth(2.0f);

    // Temporarily disable depth testing for the wireframe to be always visible
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    if (depthTestEnabled)
    {
        glDisable(GL_DEPTH_TEST);
    }

    // Draw section bounds as lines
    glBindVertexArray(currentSectionBoundsVAO);
    glDrawArrays(GL_LINES, 0, currentSectionBoundsVertexCount);

    // Restore original state
    if (depthTestEnabled)
    {
        glEnable(GL_DEPTH_TEST);
    }
    glLineWidth(originalLineWidth);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::processSection(int sx, int sy, int sz, const std::string &sectionKey)
{
    std::unordered_map<uint16_t, std::vector<BlockFace>> blockFaces;

    auto isRenderableBlock = [this](uint16_t blockId) -> bool
    {
        if (blockId == 0xFFFF)
        {
            return false;
        }

        if (std::find(noRenderBlockIds.begin(), noRenderBlockIds.end(), blockId) != noRenderBlockIds.end())
        {
            return false;
        }

        return true;
    };

    auto blockExists = [this, isRenderableBlock](int x, int y, int z) -> bool
    {
        bool outOfBounds = x < 0 || y < 0 || z < 0 || x >= region->getSizeX() || y >= region->getSizeY() || z >= region->getSizeZ();
        if (outOfBounds)
        {
            return false;
        }

        return isRenderableBlock(region->getBlockAt(x, y, z));
    };

    int sectionStartX = sx * 16;
    int sectionStartY = sy * 16;
    int sectionStartZ = sz * 16;
    int sectionEndX = std::min((sx + 1) * 16, region->getSizeX());
    int sectionEndY = std::min((sy + 1) * 16, region->getSizeY());
    int sectionEndZ = std::min((sz + 1) * 16, region->getSizeZ());

    for (int x = sectionStartX; x < sectionEndX; x++)
    {
        for (int y = sectionStartY; y < sectionEndY; y++)
        {
            for (int z = sectionStartZ; z < sectionEndZ; z++)
            {
                const uint16_t blockId = region->getBlockAt(x, y, z);

                // Skip non-renderable blocks
                if (!isRenderableBlock(blockId))
                {
                    continue;
                }

                // Check if block is visible and skip non-visible blocks
                if (!blockExists(x + 1, y, z))
                {
                    blockFaces[blockId].push_back({glm::vec3(x, y, z), 0});
                }
                if (!blockExists(x - 1, y, z))
                {
                    blockFaces[blockId].push_back({glm::vec3(x, y, z), 1});
                }
                if (!blockExists(x, y + 1, z))
                {
                    blockFaces[blockId].push_back({glm::vec3(x, y, z), 2});
                }
                if (!blockExists(x, y - 1, z))
                {
                    blockFaces[blockId].push_back({glm::vec3(x, y, z), 3});
                }
                if (!blockExists(x, y, z + 1))
                {
                    blockFaces[blockId].push_back({glm::vec3(x, y, z), 4});
                }
                if (!blockExists(x, y, z - 1))
                {
                    blockFaces[blockId].push_back({glm::vec3(x, y, z), 5});
                }
            }
        }
    }

    // Update section cache under lock
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        sectionCache[sectionKey].blockFaces = std::move(blockFaces);
        sectionCache[sectionKey].dirty = false;
        sectionCache[sectionKey].processing = false;
    }
}

void Renderer::renderSection(const std::string &sectionKey, const glm::mat4 &viewProjectionMatrix)
{
    const auto &blockFaces = sectionCache[sectionKey].blockFaces;
    for (const auto &[blockId, faces] : blockFaces)
    {
        // Skip empty groups
        if (faces.empty())
        {
            continue;
        }

        // Set block color
        glm::vec3 color;
        auto it = blockColorDict.find(blockId);
        if (it != blockColorDict.end())
        {
            color = it->second / 255.0f;
        }
        else
        {
            color = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        glUniform3fv(shadersSetup.cubeColorOverrideLoc, 1, glm::value_ptr(color));
        glUniform1i(shadersSetup.cubeUseColorOverrideLoc, 1);

        // Group faces by face type for efficient rendering
        // Organize positions by face type
        std::unordered_map<uint8_t, std::vector<glm::vec3>> facePositions;
        for (const auto &face : faces)
        {
            facePositions[face.face].push_back(face.position);
        }

        // Render each face type separately
        for (const auto &[faceType, positions] : facePositions)
        {
            if (positions.empty())
                continue;

            // Update instance buffer with positions for this face type
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STREAM_DRAW);

            // Draw only the specific face using the face indices
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(faceType * 6 * sizeof(unsigned int)), positions.size());
        }

        // Check for errors
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
        {
            std::cerr << "OpenGL error while rendering block type " << blockId << ": " << err << std::endl;
        }
    }
}

void Renderer::drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance)
{
    if (!region)
    {
        return;
    }

    // Get current camera section position
    glm::ivec3 currentSectionPos(
        floor(cameraPos.x / SECTION_SIZE),
        floor(cameraPos.y / SECTION_SIZE),
        floor(cameraPos.z / SECTION_SIZE));

    // Check if current camera moved to a new section and mark out of range sections as dirty
    bool cameraMoved = currentSectionPos != lastCameraSectionPos;
    if (cameraMoved)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        lastCameraSectionPos = currentSectionPos;

        for (auto &[key, cache] : sectionCache)
        {
            std::stringstream ss(key);
            int x, y, z;
            char comma;
            ss >> x >> comma >> y >> comma >> z;

            bool outOfRange = abs(x - currentSectionPos.x) > sectionViewDistance ||
                              abs(y - currentSectionPos.y) > sectionViewDistance ||
                              abs(z - currentSectionPos.z) > sectionViewDistance;
            if (outOfRange)
            {
                cache.dirty = true;
            }
        }
    }

    // Calculate visible section range
    int startX = std::max(0, std::min(region->getSizeX() / SECTION_SIZE, currentSectionPos.x - sectionViewDistance));
    int startY = std::max(0, std::min(region->getSizeY() / SECTION_SIZE, currentSectionPos.y - sectionViewDistance));
    int startZ = std::max(0, std::min(region->getSizeZ() / SECTION_SIZE, currentSectionPos.z - sectionViewDistance));
    int endX = std::max(0, std::min(region->getSizeX() / SECTION_SIZE, currentSectionPos.x + sectionViewDistance + 1));
    int endY = std::max(0, std::min(region->getSizeY() / SECTION_SIZE, currentSectionPos.y + sectionViewDistance + 1));
    int endZ = std::max(0, std::min(region->getSizeZ() / SECTION_SIZE, currentSectionPos.z + sectionViewDistance + 1));

    // Queue sections for processing
    for (int sx = startX; sx < endX; sx++)
    {
        for (int sy = startY; sy < endY; sy++)
        {
            for (int sz = startZ; sz < endZ; sz++)
            {
                std::string sectionKey = getSectionKey(sx, sy, sz);
                
                bool needsProcessing = false;
                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    needsProcessing = sectionCache.find(sectionKey) == sectionCache.end() || 
                                     sectionCache[sectionKey].dirty;
                }
                
                if (needsProcessing)
                {
                    // Check if this section is already being processed
                    bool alreadyProcessing = false;
                    {
                        std::lock_guard<std::mutex> lock(cacheMutex);
                        if (sectionCache.find(sectionKey) != sectionCache.end()) {
                            alreadyProcessing = sectionCache[sectionKey].processing;
                        }
                    }
                    
                    if (!alreadyProcessing) {
                        queueSectionForProcessing(sx, sy, sz, sectionKey);
                    }
                }
            }
        }
    }

    // Prepare for rendering
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    glUseProgram(shadersSetup.cubeShaderProgram);
    glUniformMatrix4fv(shadersSetup.cubeVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));
    glUniform3fv(shadersSetup.cubeLightDirLoc, 1, glm::value_ptr(lightDirection));
    glBindVertexArray(cubeVAO);

    // Only render sections that are ready
    for (int sx = startX; sx < endX; sx++)
    {
        for (int sy = startY; sy < endY; sy++)
        {
            for (int sz = startZ; sz < endZ; sz++)
            {
                std::string sectionKey = getSectionKey(sx, sy, sz);
                
                if (isSectionReady(sectionKey))
                {
                    renderSection(sectionKey, viewProjectionMatrix);
                }
            }
        }
    }

    // Cleanup
    glBindVertexArray(0);
    glUseProgram(0);
}

/*****
 ****
 *** Rendering
 ****
 ******/

void Renderer::renderFrame(int windowWidth, int windowHeight, float nearPlane, float farPlane)
{
    // Clear the screen
    glClearColor(0.82f, 0.882f, 0.933f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Create matrices
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), (float)windowWidth / (float)windowHeight, nearPlane, farPlane);
    glm::mat4 viewMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    drawAxes(viewMatrix, projectionMatrix, 0.01f);

    if (developerModeActive)
    {
        drawCurrentSectionBounds(viewMatrix, projectionMatrix);
    }

    // Draw axes
    if (region)
    {
        drawRegion(viewMatrix, projectionMatrix);
    }
}

void Renderer::startRenderLoop(Window &window)
{
    window.enableCursorCapture(true);

    while (!window.shouldClose() && isRunning)
    {
        // Calculate delta time
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        handleInput(window, deltaTime);
        renderFrame(window.getWidth(), window.getHeight());

        window.swapBuffers();
        window.pollEvents();
    }

    window.enableCursorCapture(false);
}

/*****
 ****
 *** Camera
 ****
 ******/

void Renderer::updateCameraVectors()
{
    // Calculate new front vector from yaw and pitch
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    // Recalculate right and up vectors
    cameraRight = glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f)));
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
}

/*****
 ****
 *** Input
 ****
 ******/

void Renderer::handleInput(Window &window, float deltaTime)
{
    inputHandler.handleInput(window, deltaTime);
}

/*****
 ****
 *** Setters
 ****
 ******/

void Renderer::setRegion(Region *region)
{
    this->region = region;
    sectionCache.clear();
}

void Renderer::setCameraPosition(float x, float y, float z)
{
    cameraPos = glm::vec3(x, y, z);
}

void Renderer::setLookAtPoint(float x, float y, float z)
{
    glm::vec3 lookAt(x, y, z);
    cameraFront = glm::normalize(lookAt - cameraPos);
    updateCameraVectors();
}

/*****
 ****
 *** Getters
 ****
 ******/

std::string Renderer::getSectionKey(int x, int y, int z)
{
    std::stringstream ss;
    ss << x << "," << y << "," << z;
    return ss.str();
}

void Renderer::workerFunction()
{
    while (!stopThreads) {
        std::tuple<int, int, int, std::string> task;

        // Wait for a task to be available
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return !sectionQueue.empty() || stopThreads; });
            if (stopThreads) {
                return;
            }

            task = sectionQueue.front();
            sectionQueue.pop();
        }

        // Process the task
        int sx = std::get<0>(task);
        int sy = std::get<1>(task);
        int sz = std::get<2>(task);
        std::string sectionKey = std::get<3>(task);
        processSection(sx, sy, sz, sectionKey);

        // Notify that the section is ready
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            condition.notify_all();
        }

    }
}

void Renderer::queueSectionForProcessing(int sx, int sy, int sz, const std::string &sectionKey)
{
    // Mark the section as being processed
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (sectionCache.find(sectionKey) == sectionCache.end()) {
            sectionCache[sectionKey] = SectionCache();
        }
        sectionCache[sectionKey].processing = true;
    }

    // Add to queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        sectionQueue.push(std::make_tuple(sx, sy, sz, sectionKey));
    }

    // Notify a worker
    condition.notify_one();
}

bool Renderer::isSectionReady(const std::string& sectionKey) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    if (sectionCache.find(sectionKey) == sectionCache.end()) {
        return false;
    }
    
    return !sectionCache[sectionKey].processing && !sectionCache[sectionKey].dirty;
}