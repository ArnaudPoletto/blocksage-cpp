#include "renderer/renderer.h"
#include "window.h"
#include "region.h"
#include "config.h"
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

const float initialDeveloperModeActive = false;
const glm::vec3 initialLightDirection = glm::vec3(0.2f, 1.0f, 0.7f);
const int maxNThreads = 8;

Renderer::Renderer(const std::unordered_map<uint16_t, glm::vec3> &blockColorDict, std::vector<uint16_t> noRenderBlockIds)
    : developerModeActive(initialDeveloperModeActive),
      lightDirection(glm::normalize(initialLightDirection)),
      blockColorDict(blockColorDict),
      noRenderBlockIds(noRenderBlockIds),
      stopThreads(false),
      stopDiscoveryThread(false),
      needsDiscoveryUpdate(false),
      pendingSectionViewDistance(32),
      isRunning(true),
      lastFrameTime(0.0f),
      region(nullptr),
      camera(),
      inputHandler(camera, isRunning, developerModeActive),
      shaderSetup(),
      geometrySetup()
{

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

    startSectionDiscovery();

    std::cout << "Started " << nThreads << " worker threads for section processing" << std::endl;
}

Renderer::~Renderer()
{
    // Signal threads to stop
    stopThreads = true;
    condition.notify_all();
    stopDiscoveryThread = true;
    discoveryCondition.notify_all();

    // Wait for threads to finish
    for (auto &thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    if (sectionDiscoveryThread.joinable())
    {
        sectionDiscoveryThread.join();
    }

    inputHandler.~InputHandler();
    shaderSetup.~ShaderSetup();
    geometrySetup.~GeometrySetup();
}

bool Renderer::initialize()
{
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return false;
    }

    if (!shaderSetup.initialize())
    {
        std::cerr << "Failed to set up shaders" << std::endl;
        return false;
    }

    if (!geometrySetup.initialize())
    {
        std::cerr << "Failed to set up geometry" << std::endl;
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
    glUseProgram(shaderSetup.baseShaderProgram);

    // Create model matrix
    glm::vec3 dPosition = glm::vec3(delta, delta, delta);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, -dPosition);

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    glUniformMatrix4fv(shaderSetup.axesMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

    // Enable line width (if supported by the hardware)
    GLfloat originalLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);
    glLineWidth(3.0f);

    // Draw axes as lines
    glBindVertexArray(geometrySetup.axesVAO);
    glDrawArrays(GL_LINES, 0, geometrySetup.axesVertexCount);

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

    glUseProgram(shaderSetup.baseShaderProgram);

    // Calculate current section coordinates
    float sx = floor(camera.position.x / 16) * 16;
    float sy = floor(camera.position.y / 16) * 16;
    float sz = floor(camera.position.z / 16) * 16;

    // Create model matrix to position the section bounds
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(sx, sy, sz));

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    glUniformMatrix4fv(shaderSetup.axesMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

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
    glBindVertexArray(geometrySetup.currentSectionBoundsVAO);
    glDrawArrays(GL_LINES, 0, geometrySetup.currentSectionBoundsVertexCount);

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

void Renderer::renderAllSections(
    std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::vector<glm::vec3>>> allBlockPositions,
    std::unordered_map<uint8_t, glm::vec3> allBlockColors)
{
    for (const auto &[blockId, facesPositions] : allBlockPositions)
    {
        if (facesPositions.empty())
        {
            continue;
        }

        // Set block color
        glm::vec3 color = allBlockColors[blockId];
        glUniform3fv(shaderSetup.cubeColorOverrideLoc, 1, glm::value_ptr(color));
        glUniform1i(shaderSetup.cubeUseColorOverrideLoc, 1);

        for (const auto &[faceType, positions] : facesPositions)
        {
            if (positions.empty())
            {
                continue;
            }

            // Update instance buffer with positions for this face type
            glBindBuffer(GL_ARRAY_BUFFER, geometrySetup.instanceVBO);
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
        floor(camera.position.x / SECTION_SIZE),
        floor(camera.position.y / SECTION_SIZE),
        floor(camera.position.z / SECTION_SIZE));

    // Check if current camera moved to a new section and mark out of range sections as dirty
    bool cameraMoved = currentSectionPos != lastCameraSectionPos;
    if (cameraMoved)
    {
        lastCameraSectionPos = currentSectionPos;
        triggerSectionDiscoveryUpdate(currentSectionPos, sectionViewDistance);
    }

    // Calculate visible section range
    int startX = std::max(0, std::min(region->getSizeX() / SECTION_SIZE, currentSectionPos.x - sectionViewDistance));
    int startY = std::max(0, std::min(region->getSizeY() / SECTION_SIZE, currentSectionPos.y - sectionViewDistance));
    int startZ = std::max(0, std::min(region->getSizeZ() / SECTION_SIZE, currentSectionPos.z - sectionViewDistance));
    int endX = std::max(0, std::min(region->getSizeX() / SECTION_SIZE, currentSectionPos.x + sectionViewDistance + 1));
    int endY = std::max(0, std::min(region->getSizeY() / SECTION_SIZE, currentSectionPos.y + sectionViewDistance + 1));
    int endZ = std::max(0, std::min(region->getSizeZ() / SECTION_SIZE, currentSectionPos.z + sectionViewDistance + 1));

    // Prepare for rendering
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    glUseProgram(shaderSetup.cubeShaderProgram);
    glUniformMatrix4fv(shaderSetup.cubeVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));
    glUniform3fv(shaderSetup.cubeLightDirLoc, 1, glm::value_ptr(lightDirection));
    glBindVertexArray(geometrySetup.cubeVAO);

    // Only render sections that are ready
    std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::vector<glm::vec3>>> allBlockPositions;
    std::unordered_map<uint8_t, glm::vec3> allBlockColors;
    for (int sx = startX; sx < endX; sx++)
    {
        for (int sy = startY; sy < endY; sy++)
        {
            for (int sz = startZ; sz < endZ; sz++)
            {
                std::string sectionKey = getSectionKey(sx, sy, sz);

                if (!isSectionReady(sectionKey))
                {
                    continue;
                }

                const auto &blockFaces = sectionCache[sectionKey].blockFaces;
                for (const auto &[blockId, faces] : blockFaces)
                {
                    // Skip empty groups
                    if (faces.empty())
                    {
                        continue;
                    }

                    // Get color
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

                    // Store data
                    allBlockColors[blockId] = color;
                    for (const auto &face : faces)
                    {
                        allBlockPositions[blockId][face.face].push_back(face.position);
                    }
                }
            }
        }
    }
    
    renderAllSections(allBlockPositions, allBlockColors);

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
    glm::mat4 viewMatrix = glm::lookAt(camera.position, camera.position + camera.front, camera.up);

    drawAxes(viewMatrix, projectionMatrix, 0.01f);

    if (developerModeActive)
    {
        drawCurrentSectionBounds(viewMatrix, projectionMatrix);
    }

    // Draw axes
    auto start = std::chrono::high_resolution_clock::now();
    if (region)
    {
        drawRegion(viewMatrix, projectionMatrix);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    //std::cout << "Render time: " << duration.count() << " ms" << std::endl;
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

        inputHandler.handleInput(window, deltaTime);
        renderFrame(window.getWidth(), window.getHeight());

        window.swapBuffers();
        window.pollEvents();
    }

    window.enableCursorCapture(false);
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
    while (!stopThreads)
    {
        std::tuple<int, int, int, std::string> task;

        // Wait for a task to be available
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this]
                           { return !sectionQueue.empty() || stopThreads; });
            if (stopThreads)
            {
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
        if (sectionCache.find(sectionKey) == sectionCache.end())
        {
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

bool Renderer::isSectionReady(const std::string &sectionKey)
{
    std::lock_guard<std::mutex> lock(cacheMutex);

    if (sectionCache.find(sectionKey) == sectionCache.end())
    {
        return false;
    }

    return !sectionCache[sectionKey].processing && !sectionCache[sectionKey].dirty;
}

void Renderer::startSectionDiscovery()
{
    sectionDiscoveryThread = std::thread(&Renderer::sectionDiscoveryFunction, this);
}

void Renderer::triggerSectionDiscoveryUpdate(const glm::ivec3& currentSectionPos, int sectionViewDistance)
{
    {
        std::lock_guard<std::mutex> lock(discoveryMutex);
        pendingSectionPos = currentSectionPos;
        pendingSectionViewDistance = sectionViewDistance;
        needsDiscoveryUpdate = true;
    }
    discoveryCondition.notify_one();
}

void Renderer::sectionDiscoveryFunction()
{
    while (!stopDiscoveryThread)
    {
        glm::ivec3 currentSectionPos;
        int sectionViewDistance;
        bool shouldProcess = false;
        
        // Wait for a signal to discover sections
        {
            std::unique_lock<std::mutex> lock(discoveryMutex);
            discoveryCondition.wait(lock, [this] {
                return needsDiscoveryUpdate || stopDiscoveryThread;
            });
            
            if (stopDiscoveryThread)
            {
                return;
            }
            
            currentSectionPos = pendingSectionPos;
            sectionViewDistance = pendingSectionViewDistance;
            needsDiscoveryUpdate = false;
            shouldProcess = true;
        }
        
        if (shouldProcess && region)
        {
            // Mark out of range sections as dirty
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                
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
                        bool alreadyProcessing = false;
                        
                        {
                            std::lock_guard<std::mutex> lock(cacheMutex);
                            needsProcessing = sectionCache.find(sectionKey) == sectionCache.end() ||
                                             sectionCache[sectionKey].dirty;
                                             
                            if (needsProcessing && sectionCache.find(sectionKey) != sectionCache.end())
                            {
                                alreadyProcessing = sectionCache[sectionKey].processing;
                            }
                        }

                        if (needsProcessing && !alreadyProcessing)
                        {
                            queueSectionForProcessing(sx, sy, sz, sectionKey);
                        }
                    }
                }
            }
        }
    }
}