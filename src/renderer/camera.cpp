#include "opengl_headers.h"
#include "renderer/camera.h"

const glm::vec3 initialPosition = glm::vec3(0.0f, 0.0f, 0.0f);
const glm::vec3 initialFront = glm::vec3(0.0f, 0.0f, -1.0f);
const glm::vec3 initialUp = glm::vec3(0.0f, 1.0f, 0.0f);
const glm::vec3 initialRight = glm::vec3(1.0f, 0.0f, 0.0f);
const float initialYaw = -90.0f;
const float initialPitch = 0.0f;

Camera::Camera()
    : position(initialPosition),
      front(initialFront),
      up(initialUp),
      right(initialRight),
      yaw(initialYaw),
      pitch(initialPitch)
{
    updateCameraVectors();
}

Camera::~Camera() {}

void Camera::updateCameraVectors()
{
    // Calculate new front vector from yaw and pitch
    glm::vec3 cameraFront;
    cameraFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront.y = sin(glm::radians(pitch));
    cameraFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(cameraFront);

    // Recalculate right and up vectors
    right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::setCameraPosition(float x, float y, float z)
{
    position = glm::vec3(x, y, z);
    updateCameraVectors();
}

void Camera::setLookAtPoint(float x, float y, float z)
{
    glm::vec3 lookAt(x, y, z);
    front = glm::normalize(lookAt - position);
    updateCameraVectors();
}

