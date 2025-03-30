#pragma once

#include <vector>

class GeometrySetup
{
public:
    GeometrySetup();
    ~GeometrySetup();
    bool initialize();

    GLuint axesVAO;
    GLuint currentSectionBoundsVAO;
    GLuint cubeVAO;
    GLuint instanceVBO;
    int axesVertexCount;
    int currentSectionBoundsVertexCount;

private:
    bool setupCubeGeometry();
    bool setupAxesGeometry();
    bool setupCurrentSectionBoundsGeometry();
    
    GLuint axesVBO;
    GLuint currentSectionBoundsVBO;
    GLuint cubeVBO;
    GLuint cubeEBO;
    GLuint faceFlagsVBO;
    int cubeVertexCount;
    std::vector<unsigned int> cubeFaceIndices[6];
};