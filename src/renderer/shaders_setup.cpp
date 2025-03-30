#include "renderer/shader_setup.h"
#include "renderer/shader_sources.h"
#include <iostream>

ShaderSetup::ShaderSetup()
    : baseShaderProgram(NULL),
      cubeShaderProgram(NULL),
      axesMVPMatrixLoc(NULL),
      cubeVPMatrixLoc(NULL),
      cubeColorOverrideLoc(NULL),
      cubeUseColorOverrideLoc(NULL),
      cubeLightDirLoc(NULL)
{
}

ShaderSetup::~ShaderSetup()
{
    // Clean up axes
    glDeleteProgram(baseShaderProgram);

    // Clean up cube
    glDeleteProgram(cubeShaderProgram);
}

bool ShaderSetup::initialize()
{
    baseShaderProgram = createShaderProgram(baseVertexShaderSource, axesFragmentShaderSource);

    // Axes shader
    if (baseShaderProgram == 0)
    {
        std::cerr << "Failed to create base shader program" << std::endl;
        return false;
    }
    axesMVPMatrixLoc = glGetUniformLocation(baseShaderProgram, "mvpMatrix");

    // Cube shader
    cubeShaderProgram = createShaderProgram(cubeVertexShaderSource, cubeFragmentShaderSource);
    if (cubeShaderProgram == 0)
    {
        std::cerr << "Failed to create cube shader program" << std::endl;
        return false;
    }

    cubeVPMatrixLoc = glGetUniformLocation(cubeShaderProgram, "viewProjectionMatrix");
    cubeColorOverrideLoc = glGetUniformLocation(cubeShaderProgram, "colorOverride");
    cubeUseColorOverrideLoc = glGetUniformLocation(cubeShaderProgram, "useColorOverride");
    cubeLightDirLoc = glGetUniformLocation(cubeShaderProgram, "lightDir");
    if (cubeVPMatrixLoc == -1 || cubeColorOverrideLoc == -1 || cubeUseColorOverrideLoc == -1 || cubeLightDirLoc == -1)
    {
        std::cerr << "Uniforms not found in cube shader program" << std::endl;
        return false;
    }

    return true;
}

GLuint ShaderSetup::compileShader(GLenum shaderType, const char *source)
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

GLuint ShaderSetup::createShaderProgram(const char *vertexShaderSource, const char *fragmentShaderSource)
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