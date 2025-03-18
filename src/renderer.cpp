#include "renderer.h"
#include "window.h"
#include "region.h"
#include "config.h"
#include <iostream>
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

Renderer::Renderer(const std::unordered_map<uint16_t, glm::vec3> &blockColorDict, std::vector<uint16_t> noRenderBlockIds)
    : cameraPos(glm::vec3(0.0f, 0.0f, 0.0f)),
      cameraFront(glm::vec3(0.0f, 0.0f, -1.0f)),
      cameraUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f),
      pitch(0.0f),
      moveSpeed(5.0f),
      moveSpeedIncreaseFactor(5.0f),
      mouseSensitivity(0.1f),
      isRunning(true),
      developerModeActive(false),
      deltaTime(0.0f),
      lastFrameTime(0.0f),
      region(nullptr),
      blockColorDict(blockColorDict),
      noRenderBlockIds(noRenderBlockIds),
      lightDirection(glm::normalize(glm::vec3(0.2f, 1.0f, 0.7f)))
{
    updateCameraVectors();
}

Renderer::~Renderer()
{
    // Clean up axes
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);
    glDeleteProgram(baseShaderProgram);

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
 *** Shaders
 ****
 ******/

const char *baseVertexShaderSource = R"(
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

// const char *cubeVertexShaderSource = R"(
// #version 460 core
// layout (location = 0) in vec3 aPos;
// layout (location = 1) in vec3 aColor;
// layout (location = 2) in vec3 instancePos;

// out vec3 vertexColor;

// uniform mat4 viewProjectionMatrix;

// void main() {
//     mat4 model = mat4(1.0);
//     model[3] = vec4(instancePos, 1.0);

//     gl_Position = viewProjectionMatrix * model * vec4(aPos, 1.0);
//     vertexColor = aColor;
// }
// )";

const char *cubeVertexShaderSource = R"(
    #version 460 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    layout (location = 2) in vec3 instancePos;
    layout (location = 3) in vec3 aNormal;  // Add normal attribute
    
    out vec3 vertexColor;
    out vec3 fragNormal;   // Pass normal to fragment shader
    out vec3 fragPos;      // Pass fragment position for lighting calculations
    
    uniform mat4 viewProjectionMatrix;
    
    void main() {
        mat4 model = mat4(1.0);
        model[3] = vec4(instancePos, 1.0);
        
        // Calculate world position
        vec4 worldPos = model * vec4(aPos, 1.0);
        gl_Position = viewProjectionMatrix * worldPos;
        
        // Pass values to fragment shader
        vertexColor = aColor;
        fragNormal = mat3(model) * aNormal;  // Transform normal to world space
        fragPos = worldPos.xyz;
    }
    )";

// const char *cubeFragmentShaderSource = R"(
//     #version 460 core
//     in vec3 vertexColor;
//     out vec4 FragColor;

//     uniform vec3 colorOverride;
//     uniform bool useColorOverride;

//     void main() {
//         if (useColorOverride) {
//             FragColor = vec4(colorOverride, 1.0);
//         } else {
//             FragColor = vec4(vertexColor, 1.0);
//         }
//     }
//     )";

const char *cubeFragmentShaderSource = R"(
    #version 460 core
    in vec3 vertexColor;
    in vec3 fragNormal;   // Receive normal from vertex shader
    in vec3 fragPos;      // Receive fragment position
    
    out vec4 FragColor;
    
    uniform vec3 colorOverride;
    uniform bool useColorOverride;
    uniform vec3 lightDir;    // Direction of the light
    uniform vec3 viewPos;     // Camera position for optional specular highlights
    
    void main() {
        // Normalize the incoming normal
        vec3 normal = normalize(fragNormal);
        
        // Base color (either from vertex color or override)
        vec3 baseColor = useColorOverride ? colorOverride : vertexColor;
        
        // Calculate lighting
        // 1. Ambient component (constant light)
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * baseColor;
        
        // 2. Diffuse component (directional light)
        vec3 lightDirection = normalize(lightDir);
        float diff = max(dot(normal, lightDirection), 0.0);
        vec3 diffuse = diff * baseColor;
        
        // Combine lighting components
        vec3 finalColor = ambient + diffuse;
        
        // Output the final color
        FragColor = vec4(finalColor, 1.0);
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
    baseShaderProgram = createShaderProgram(baseVertexShaderSource, axesFragmentShaderSource);

    // Axes shader
    if (baseShaderProgram == 0)
    {
        return false;
    }
    axesMVPMatrixLoc = glGetUniformLocation(baseShaderProgram, "mvpMatrix");

    // Current section bounds shader

    // Cube shader
    cubeShaderProgram = createShaderProgram(cubeVertexShaderSource, cubeFragmentShaderSource);
    if (cubeShaderProgram == 0)
    {
        return false;
    }

    cubeVPMatrixLoc = glGetUniformLocation(cubeShaderProgram, "viewProjectionMatrix");
    cubeColorOverrideLoc = glGetUniformLocation(cubeShaderProgram, "colorOverride");
    cubeUseColorOverrideLoc = glGetUniformLocation(cubeShaderProgram, "useColorOverride");
    cubeLightDirLoc = glGetUniformLocation(cubeShaderProgram, "lightDir");
    // cubeViewPosLoc = glGetUniformLocation(cubeShaderProgram, "viewPos");
    if (cubeVPMatrixLoc == -1 || cubeColorOverrideLoc == -1 || cubeUseColorOverrideLoc == -1 || cubeLightDirLoc == -1)
    {
        std::cerr << "Uniforms not found in cube shader program" << std::endl;
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
        // Front face (Z+)
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-left
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-right
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-left

        // Back face (Z-)
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Bottom-left
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Bottom-right
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Top-right
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, // Top-left

        // Left face (X-)
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Bottom-back
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Bottom-front
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Top-front
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, // Top-back

        // Right face (X+)
        1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Bottom-front
        1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Bottom-back
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Top-back
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, // Top-front

        // Bottom face (Y-)
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Back-left
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Back-right
        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Front-right
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, // Front-left

        // Top face (Y+)
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // Front-left
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // Front-right
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // Back-right
        0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f  // Back-left
    };

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
    glUseProgram(baseShaderProgram);

    // Create model matrix
    glm::vec3 dPosition = glm::vec3(delta, delta, delta);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, -dPosition);

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    glUniformMatrix4fv(axesMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

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

    glUseProgram(baseShaderProgram);

    // Calculate current section coordinates
    float sx = floor(cameraPos.x / 16) * 16;
    float sy = floor(cameraPos.y / 16) * 16;
    float sz = floor(cameraPos.z / 16) * 16;

    // Create model matrix to position the section bounds
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(sx, sy, sz));

    // Compute MVP matrix
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    glUniformMatrix4fv(axesMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

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

// void Renderer::drawRegion(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix, int sectionViewDistance)
// {
//     if (!region)
//     {
//         return;
//     }

//     // Group blocks by ID
//     std::unordered_map<uint16_t, std::vector<glm::vec3>> blockGroups;

//     // Collect visible blocks
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

//     // Collect positions for each block type
//     for (int x = startX; x < endX; x++)
//     {
//         for (int y = startY; y < endY; y++)
//         {
//             for (int z = startZ; z < endZ; z++)
//             {
//                 const uint16_t blockId = region->getBlockAt(x, y, z);
//                 // Skip air blocks
//                 if (blockId == 0xFFFF)
//                 {
//                     continue;
//                 }

//                 // Skip non-renderable blocks
//                 if (std::find(noRenderBlockIds.begin(), noRenderBlockIds.end(), blockId) != noRenderBlockIds.end())
//                 {
//                     continue;
//                 }

//                 blockGroups[blockId].push_back(glm::vec3(x, y, z));
//             }
//         }
//     }

//     // Now render each block type in a single batch
//     glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
//     glUseProgram(cubeShaderProgram);

//     // Set combined view-projection matrix
//     glUniformMatrix4fv(cubeVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));

//     // Set lighting uniforms
//     glUniform3fv(cubeLightDirLoc, 1, glm::value_ptr(lightDirection));
//     //glUniform3fv(cubeViewPosLoc, 1, glm::value_ptr(cameraPos));

//     // Bind cube VAO
//     glBindVertexArray(cubeVAO);

//     // Draw each block type in one batch
//     for (const auto &[blockId, positions] : blockGroups)
//     {
//         // Set block color
//         glm::vec3 color;
//         auto it = blockColorDict.find(blockId);
//         if (it != blockColorDict.end())
//         {
//             color = it->second;
//             color = color / 255.0f;
//         }
//         else
//         {
//             color = glm::vec3(0.0f, 0.0f, 0.0f);
//         }
//         glUniform3fv(cubeColorOverrideLoc, 1, glm::value_ptr(color));
//         glUniform1i(cubeUseColorOverrideLoc, 1);

//         // Update instance buffer with all positions for this block type
//         glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
//         glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STREAM_DRAW);

//         // Draw all instances in one call
//         glDrawElementsInstanced(GL_TRIANGLES, cubeVertexCount, GL_UNSIGNED_INT, 0, positions.size());

//         // Check for errors
//         GLenum err = glGetError();
//         if (err != GL_NO_ERROR)
//         {
//             std::cerr << "OpenGL error after drawing: " << err << std::endl;
//         }
//     }

//     // Cleanup
//     glBindVertexArray(0);
//     glUseProgram(0);
// }

void Renderer::processSection(int sx, int sy, int sz, const std::string &sectionKey)
{
    // Create new cache entry for this section
    SectionCache newCache;
    newCache.dirty = false;

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
                bool visible = false;
                if (x == 0 || !isRenderableBlock(region->getBlockAt(x - 1, y, z)))
                {
                    visible = true;
                }
                else if (x == region->getSizeX() - 1 || !isRenderableBlock(region->getBlockAt(x + 1, y, z)))
                {
                    visible = true;
                }
                else if (y == 0 || !isRenderableBlock(region->getBlockAt(x, y - 1, z)))
                {
                    visible = true;
                }
                else if (y == region->getSizeY() - 1 || !isRenderableBlock(region->getBlockAt(x, y + 1, z)))
                {
                    visible = true;
                }
                else if (z == 0 || !isRenderableBlock(region->getBlockAt(x, y, z - 1)))
                {
                    visible = true;
                }
                else if (z == region->getSizeZ() - 1 || !isRenderableBlock(region->getBlockAt(x, y, z + 1)))
                {
                    visible = true;
                }
                
                if (!visible)
                {
                    continue;
                }

                // Add block to the appropriate group
                newCache.blockGroups[blockId].push_back(glm::vec3(x, y, z));
            }
        }
    }

    // Update cache
    sectionCache[sectionKey] = newCache;
}

void Renderer::renderSection(const std::string &sectionKey, const glm::mat4 &viewProjectionMatrix)
{
    const auto &blockGroups = sectionCache[sectionKey].blockGroups;
    for (const auto &[blockId, positions] : blockGroups)
    {
        // Skip empty groups
        if (positions.empty())
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
        glUniform3fv(cubeColorOverrideLoc, 1, glm::value_ptr(color));
        glUniform1i(cubeUseColorOverrideLoc, 1);

        // Update instance buffer with positions for this block type
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STREAM_DRAW);

        // Draw all instances in one call
        glDrawElementsInstanced(GL_TRIANGLES, cubeVertexCount, GL_UNSIGNED_INT, 0, positions.size());

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
        floor(cameraPos.x / 16),
        floor(cameraPos.y / 16),
        floor(cameraPos.z / 16));

    // Check if current camera moved to a new section and mark out of range sections as dirty
    bool cameraMoved = currentSectionPos != lastCameraSectionPos;
    if (cameraMoved)
    {
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

    // Prepare for rendering
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    glUseProgram(cubeShaderProgram);
    glUniformMatrix4fv(cubeVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));
    glUniform3fv(cubeLightDirLoc, 1, glm::value_ptr(lightDirection));
    glBindVertexArray(cubeVAO);

    int startX = std::max(0, std::min(region->getSizeX() / 16, currentSectionPos.x - sectionViewDistance));
    int startY = std::max(0, std::min(region->getSizeY() / 16, currentSectionPos.y - sectionViewDistance));
    int startZ = std::max(0, std::min(region->getSizeZ() / 16, currentSectionPos.z - sectionViewDistance));
    int endX = std::max(0, std::min(region->getSizeX() / 16, currentSectionPos.x + sectionViewDistance + 1));
    int endY = std::max(0, std::min(region->getSizeY() / 16, currentSectionPos.y + sectionViewDistance + 1));
    int endZ = std::max(0, std::min(region->getSizeZ() / 16, currentSectionPos.z + sectionViewDistance + 1));

    for (int sx = startX; sx < endX; sx++)
    {
        for (int sy = startY; sy < endY; sy++)
        {
            for (int sz = startZ; sz < endZ; sz++)
            {
                std::string sectionKey = getSectionKey(sx, sy, sz);
                // Process section if needed
                if (sectionCache.find(sectionKey) == sectionCache.end() || sectionCache[sectionKey].dirty)
                {
                    processSection(sx, sy, sz, sectionKey);
                }

                // Render section
                renderSection(sectionKey, viewProjectionMatrix);
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

    // Developer mode
    static bool developerKeyPressed = false;
    if (window.isKeyPressed(GLFW_KEY_H))
    {
        if (!developerKeyPressed)
        {
            developerKeyPressed = true;
            developerModeActive = !developerModeActive;
            std::cout << "Developer mode: " << (developerModeActive ? "enabled" : "disabled") << std::endl;
        }
    }
    else
    {
        developerKeyPressed = false;
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