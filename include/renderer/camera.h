#pragma once

#include "opengl_headers.h"

class Camera
{
public:
    Camera();
    ~Camera();

    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    float yaw;
    float pitch;

    void updateCameraVectors();
    void setCameraPosition(float x, float y, float z);
    void setLookAtPoint(float x, float y, float z);
};