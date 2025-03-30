#include "opengl_headers.h"
#include "renderer/geometry_setup.h"
#include <iostream>
#include <vector>


GeometrySetup::GeometrySetup()
    : axesVAO(0),
      axesVBO(0),
      currentSectionBoundsVAO(0),
      currentSectionBoundsVBO(0),
      cubeVAO(0),
      cubeVBO(0),
      cubeEBO(0),
      instanceVBO(0),
      faceFlagsVBO(0),
      axesVertexCount(0),
      currentSectionBoundsVertexCount(0),
      cubeVertexCount(0)
{
    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_BACK);
}

GeometrySetup::~GeometrySetup()
{
    // Clean up axes
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);

    // Clean up cube
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
}

bool GeometrySetup::initialize()
{
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

bool GeometrySetup::setupAxesGeometry()
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

bool GeometrySetup::setupCurrentSectionBoundsGeometry()
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

bool GeometrySetup::setupCubeGeometry()
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