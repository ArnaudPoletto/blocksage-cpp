#include "renderer.h"
#include "window.h"
#include "region.h"
#include "config.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <nlohmann/json.hpp>
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*****
 ****
 *** Constructor and destructor
 ****
 ******/

Renderer::Renderer(const std::unordered_map<uint16_t, glm::vec3> &blockColorDict, std::vector<uint16_t> noRenderBlockIds)
    : cameraPos(glm::vec3(0.0f, 0.0f, 0.0f)),
      cameraFront(glm::vec3(0.0f, 0.0f, -1.0f)),
      cameraUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f),
      pitch(0.0f),
      moveSpeed(5.0f),
      moveSpeedIncreaseFactor(2.0f),
      mouseSensitivity(0.1f),
      isRunning(true),
      deltaTime(0.0f),
      lastFrameTime(0.0f),
      region(nullptr),
      blockColorDict(blockColorDict),
      noRenderBlockIds(noRenderBlockIds)
{
    updateCameraVectors();
}

Renderer::~Renderer()
{
    // Clean up axes
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);
    glDeleteProgram(axesShaderProgram);

    // Clean up cube
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteProgram(cubeShaderProgram);
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

    if (!setupShaders())
    {
        std::cerr << "Failed to set up shaders" << std::endl;
        return false;
    }

    if (!setupCubeGeometry())
    {
        std::cerr << "Failed to set up cube geometry" << std::endl;
        return false;
    }

    if (!setupAxesGeometry())
    {
        std::cerr << "Failed to set up axes geometry" << std::endl;
        return false;
    }

    return true;
}

/*****
 ****
 *** Shaders
 ****
 ******/

const char *axesVertexShaderSource = R"(
    #version 460 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    
    out vec3 vertexColor;
    
    uniform mat4 mvpMatrix;
    
    void main() {
        gl_Position = mvpMatrix * vec4(aPos, 1.0);
        vertexColor = aColor;
    }
    )";

const char *axesFragmentShaderSource = R"(
    #version 460 core
    in vec3 vertexColor;
    out vec4 FragColor;
    
    void main() {
        FragColor = vec4(vertexColor, 1.0);
    }
    )";

// const char *cubeVertexShaderSource = R"(
// #version 460 core
// layout (location = 0) in vec3 aPos;
// layout (location = 1) in vec3 aColor;

// out vec3 vertexColor;

// uniform mat4 mvpMatrix;

// void main() {
//     gl_Position = mvpMatrix * vec4(aPos, 1.0);
//     vertexColor = aColor;
// }
// )";

const char *batchedCubeVertexShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 instancePos;

out vec3 vertexColor;

uniform mat4 viewProjectionMatrix;

void main() {
    mat4 model = mat4(1.0);
    model[3] = vec4(instancePos, 1.0);

    gl_Position = viewProjectionMatrix * model * vec4(aPos, 1.0);
    vertexColor = aColor;
}
)";

const char *cubeFragmentShaderSource = R"(
    #version 460 core
    in vec3 vertexColor;
    out vec4 FragColor;
    
    uniform vec3 colorOverride;
    uniform bool useColorOverride;
    
    void main() {
        if (useColorOverride) {
            FragColor = vec4(colorOverride, 1.0);
        } else {
            FragColor = vec4(vertexColor, 1.0);
        }
    }
    )";

GLuint Renderer::compileShader(GLenum shaderType, const char *source)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        return 0;
    }

    return shader;
}

GLuint Renderer::createShaderProgram(const char *vertexShaderSource, const char *fragmentShaderSource)
{
    // Compile vertex shader
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (vertexShader == 0)
    {
        std::cerr << "Failed to compile vertex shader" << std::endl;
        return 0;
    }

    // Compile fragment shader
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (fragmentShader == 0)
    {
        std::cerr << "Failed to compile fragment shader" << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }

    // Create shader program
    GLuint shaderProgram = glCreateProgram();
    if (shaderProgram == 0)
    {
        std::cerr << "Failed to create shader program" << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    // Attach shaders and link program
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    GLint success;
    GLchar infoLog[1024];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 1024, NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram);
        return 0;
    }

    // Detach and delete shaders after successful linking
    glDetachShader(shaderProgram, vertexShader);
    glDetachShader(shaderProgram, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

bool Renderer::setupShaders()
{
    axesShaderProgram = createShaderProgram(axesVertexShaderSource, axesFragmentShaderSource);
    if (axesShaderProgram == 0)
    {
        return false;
    }

    // cubeShaderProgram = createShaderProgram(cubeVertexShaderSource, cubeFragmentShaderSource);
    cubeShaderProgram = createShaderProgram(batchedCubeVertexShaderSource, cubeFragmentShaderSource);
    if (cubeShaderProgram == 0)
    {
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

bool Renderer::setupCubeGeometry()
{
    // Create cube geometry
    std::vector<float> vertices = {
        // Front face
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Bottom-left
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Top-right
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Top-left

        // Back face
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Bottom-left
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Bottom-right
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Top-right
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // Top-left

        // Left face
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-back
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Bottom-front
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Top-front
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-back

        // Right face
        1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Bottom-front
        1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, // Bottom-back
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, // Top-back
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Top-front

        // Bottom face
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Back-left
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Back-right
        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, // Front-right
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, // Front-left

        // Top face
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, // Front-left
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, // Front-right
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, // Back-right
        0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f  // Back-left
    };

    // Indices for drawing triangles
    std::vector<unsigned int> indices = {
        // Front face
        0, 1, 2,
        2, 3, 0,
        // Back face
        4, 5, 6,
        6, 7, 4,
        // Left face
        8, 9, 10,
        10, 11, 8,
        // Right face
        12, 13, 14,
        14, 15, 12,
        // Bottom face
        16, 17, 18,
        18, 19, 16,
        // Top face
        20, 21, 22,
        22, 23, 20};

    cubeVertexCount = indices.size();

    // Create VAO, VBO, and EBO
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glGenBuffers(1, &instanceVBO);

    // Bind VAO
    glBindVertexArray(cubeVAO);

    // Bind VBO and upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Set up vertex attributes from VBO
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

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
    glUseProgram(axesShaderProgram);

    // Create model matrix
    glm::vec3 dPosition = glm::vec3(delta, delta, delta);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, -dPosition);

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    GLint mvpMatrixLocation = glGetUniformLocation(axesShaderProgram, "mvpMatrix");
    glUniformMatrix4fv(mvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

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

void Renderer::drawCube(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix,
                        glm::vec3 position, glm::vec3 color)
{
    // Activate shader program
    glUseProgram(cubeShaderProgram);

    // Create model matrix for positioning the cube
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);

    // Calculate the MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;

    // Get uniform locations
    GLint mvpMatrixLocation = glGetUniformLocation(cubeShaderProgram, "mvpMatrix");
    GLint colorOverrideLocation = glGetUniformLocation(cubeShaderProgram, "colorOverride");
    GLint useColorOverrideLocation = glGetUniformLocation(cubeShaderProgram, "useColorOverride");

    // Set uniforms
    glUniformMatrix4fv(mvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
    glUniform3fv(colorOverrideLocation, 1, glm::value_ptr(color));
    glUniform1i(useColorOverrideLocation, 1); // Enable color override

    // Bind VAO and draw cube
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, cubeVertexCount, GL_UNSIGNED_INT, 0);

    // Reset state
    glUniform1i(useColorOverrideLocation, 0); // Disable color override
    glBindVertexArray(0);
    glUseProgram(0);
}

// void Renderer::drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance)
// {
//     if (!region)
//     {
//         return;
//     }

//     glm::vec3 defaultColor(0.0f, 0.0f, 0.0f);

//     glm::vec3 sectionCameraPos = cameraPos;
//     sectionCameraPos.x = floor(sectionCameraPos.x / 16) * 16;
//     sectionCameraPos.y = floor(sectionCameraPos.y / 16) * 16;
//     sectionCameraPos.z = floor(sectionCameraPos.z / 16) * 16;
//     int startX = std::max(0, std::min(region->getSizeX() - 16, (int)sectionCameraPos.x - sectionViewDistance * 16));
//     int startY = std::max(0, std::min(region->getSizeY() - 16, (int)sectionCameraPos.y - sectionViewDistance * 16));
//     int startZ = std::max(0, std::min(region->getSizeZ() - 16, (int)sectionCameraPos.z - sectionViewDistance * 16));
//     int endX = std::max(0, std::min(region->getSizeX() - 16, (int)sectionCameraPos.x + sectionViewDistance * 16));
//     int endY = std::max(0, std::min(region->getSizeY() - 16, (int)sectionCameraPos.y + sectionViewDistance * 16));
//     int endZ = std::max(0, std::min(region->getSizeZ() - 16, (int)sectionCameraPos.z + sectionViewDistance * 16));

//     for (int x = startX; x < endX; x++)
//     {
//         for (int y = startY; y < endY; y++)
//         {
//             for (int z = startZ; z < endZ; z++)
//             {
//                 const uint16_t blockId = region->getBlockAt(x, y, z);
//                 if (blockId == 0xFFFF)
//                 {
//                     continue;
//                 }

//                 // Do not render non-renderable blocks
//                 if (std::find(noRenderBlockIds.begin(), noRenderBlockIds.end(), blockId) != noRenderBlockIds.end())
//                 {
//                     continue;
//                 }

//                 // Get color from block color dictionary
//                 glm::vec3 color;
//                 auto it = blockColorDict.find(blockId);
//                 if (it != blockColorDict.end())
//                 {
//                     color = it->second;
//                     color = color / 255.0f;
//                 }
//                 else
//                 {
//                     color = defaultColor;
//                 }
//                 glm::vec3 position(x, y, z);
//                 drawCube(viewMatrix, projectionMatrix, position, color);
//             }
//         }
//     }
// }

void Renderer::drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance)
{
    if (!region)
    {
        return;
    }

    // Group blocks by ID
    std::unordered_map<uint16_t, std::vector<glm::vec3>> blockGroups;

    // Collect visible blocks
    glm::vec3 sectionCameraPos = cameraPos;
    sectionCameraPos.x = floor(sectionCameraPos.x / 16) * 16;
    sectionCameraPos.y = floor(sectionCameraPos.y / 16) * 16;
    sectionCameraPos.z = floor(sectionCameraPos.z / 16) * 16;
    int startX = std::max(0, std::min(region->getSizeX() - 16, (int)sectionCameraPos.x - sectionViewDistance * 16));
    int startY = std::max(0, std::min(region->getSizeY() - 16, (int)sectionCameraPos.y - sectionViewDistance * 16));
    int startZ = std::max(0, std::min(region->getSizeZ() - 16, (int)sectionCameraPos.z - sectionViewDistance * 16));
    int endX = std::max(0, std::min(region->getSizeX() - 16, (int)sectionCameraPos.x + sectionViewDistance * 16));
    int endY = std::max(0, std::min(region->getSizeY() - 16, (int)sectionCameraPos.y + sectionViewDistance * 16));
    int endZ = std::max(0, std::min(region->getSizeZ() - 16, (int)sectionCameraPos.z + sectionViewDistance * 16));

    // Collect positions for each block type
    for (int x = startX; x < endX; x++)
    {
        for (int y = startY; y < endY; y++)
        {
            for (int z = startZ; z < endZ; z++)
            {
                const uint16_t blockId = region->getBlockAt(x, y, z);
                // Skip air blocks
                if (blockId == 0xFFFF)
                {
                    continue;
                }

                // Skip non-renderable blocks
                if (std::find(noRenderBlockIds.begin(), noRenderBlockIds.end(), blockId) != noRenderBlockIds.end())
                {
                    continue;
                }

                blockGroups[blockId].push_back(glm::vec3(x, y, z));
            }
        }
    }

    // Now render each block type in a single batch
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    glUseProgram(cubeShaderProgram);

    // Set combined view-projection matrix
    GLint vpMatrixLoc = glGetUniformLocation(cubeShaderProgram, "viewProjectionMatrix");
    if (vpMatrixLoc == -1)
    {
        std::cerr << "Uniform 'viewProjectionMatrix' not found in shader!" << std::endl;
    }
    glUniformMatrix4fv(vpMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));

    // Bind cube VAO
    glBindVertexArray(cubeVAO);

    // Draw each block type in one batch
    for (const auto &[blockId, positions] : blockGroups)
    {
        // Set block color
        glm::vec3 color;
        auto it = blockColorDict.find(blockId);
        if (it != blockColorDict.end())
        {
            color = it->second;
            color = color / 255.0f;
        }
        else
        {
            color = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        GLint colorLoc = glGetUniformLocation(cubeShaderProgram, "colorOverride");
        glUniform3fv(colorLoc, 1, glm::value_ptr(color));
        glUniform1i(glGetUniformLocation(cubeShaderProgram, "useColorOverride"), 1);

        // Update instance buffer with all positions for this block type
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STREAM_DRAW);

        // Draw all instances in one call
        glDrawElementsInstanced(GL_TRIANGLES, cubeVertexCount, GL_UNSIGNED_INT, 0, positions.size());

        // Check for errors
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
        {
            std::cerr << "OpenGL error after drawing: " << err << std::endl;
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

void Renderer::renderFrame(int windowWidth, int windowHeight)
{
    // Clear the screen
    glClearColor(0.82f, 0.882f, 0.933f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Create matrices
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f),
                                                  (float)windowWidth / (float)windowHeight,
                                                  0.1f, 100.0f);
    glm::mat4 viewMatrix = glm::lookAt(cameraPos,
                                       cameraPos + cameraFront,
                                       cameraUp);

    if (region)
    {
        drawRegion(viewMatrix, projectionMatrix, 6);
    }

    drawAxes(viewMatrix, projectionMatrix);
}

void Renderer::startRenderLoop(Window &window)
{
    window.enableCursorCapture(true);

    while (!window.shouldClose() && isRunning)
    {
        // Calculate delta time
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        handleInput(window);
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

void Renderer::handleInput(Window &window)
{
    // Calculate movement speed based on whether shift is pressed
    float speed = moveSpeed;
    if (window.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
    {
        speed *= moveSpeedIncreaseFactor;
    }

    // Adjust for framerate independence
    float distance = speed * deltaTime;

    // Forward/backward movement
    if (window.isKeyPressed(GLFW_KEY_W))
    {
        cameraPos += cameraFront * distance;
    }
    if (window.isKeyPressed(GLFW_KEY_S))
    {
        cameraPos -= cameraFront * distance;
    }

    // Left/right movement
    if (window.isKeyPressed(GLFW_KEY_A))
    {
        cameraPos -= cameraRight * distance;
    }
    if (window.isKeyPressed(GLFW_KEY_D))
    {
        cameraPos += cameraRight * distance;
    }

    // Up/down movement
    if (window.isKeyPressed(GLFW_KEY_SPACE))
    {
        cameraPos += cameraUp * distance;
    }
    if (window.isKeyPressed(GLFW_KEY_Q))
    {
        cameraPos -= cameraUp * distance;
    }

    // Exit
    if (window.isKeyPressed(GLFW_KEY_ESCAPE))
    {
        isRunning = false;
    }

    // Mouse look
    processMouse(window);
}

void Renderer::processMouse(Window &window)
{
    double dx, dy;
    window.getMouseDelta(dx, dy);

    dx *= mouseSensitivity;
    dy *= mouseSensitivity;

    yaw += dx;
    pitch += dy; // Inverted controls

    // Constrain pitch to avoid gimbal lock
    if (pitch > 89.0f)
    {
        pitch = 89.0f;
    }
    if (pitch < -89.0f)
    {
        pitch = -89.0f;
    }

    updateCameraVectors();
}

/*****
 ****
 *** Setters
 ****
 ******/

void Renderer::setRegion(Region *region)
{
    this->region = region;
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