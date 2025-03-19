#pragma once

#include "opengl_headers.h"

class ShadersSetup
{
public:
    ShadersSetup();
    ~ShadersSetup();
    bool initialize();

    GLuint baseShaderProgram;
    GLuint cubeShaderProgram;

    GLint axesMVPMatrixLoc;
    GLint cubeVPMatrixLoc;
    GLint cubeColorOverrideLoc;
    GLint cubeUseColorOverrideLoc;
    GLint cubeLightDirLoc;
private:

    GLuint createShaderProgram(const char *vertexShaderSource, const char *fragmentShaderSource);
    GLuint compileShader(GLenum shaderType, const char *source);
};