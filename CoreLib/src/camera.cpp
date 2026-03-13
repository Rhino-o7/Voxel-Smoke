#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "runtime_state.h"

namespace yc {

Camera::Camera() {}

void Camera::init() {
    // Start from a neutral orientation at world origin.
    m_pitch = 0;
    m_yaw = 0;
    m_position = glm::vec3(0);

    updateDirection();
    updateMatrix();
}

void Camera::setFovDeg(float value) {
    m_fovDeg = std::clamp(value, 10.0f, 140.0f);
    update();
    updateMatrix();
}

void Camera::update() {
    // Keep projection matrix aligned with current viewport aspect ratio.
    float screenRatio = 1.0f * yc::runtime_state::GetViewportWidth() / yc::runtime_state::GetViewportHeight();
    m_projectionMatrix = glm::perspective(glm::radians(m_fovDeg), screenRatio, 0.1f, 1000.0f);
}

glm::mat4 Camera::getViewMatrix() const {
    return m_viewMatrix;
}

glm::mat4 Camera::getProjectionMatrix() const {
    return m_projectionMatrix;
}

glm::mat4 Camera::getProjectionViewMatrix() const {
    return m_projectionViewMatrix;
}

float Camera::getPitch() const {
    return m_pitch;
}

float Camera::getYaw() const {
    return m_yaw;
}

glm::vec3 Camera::getRight() const {
    return m_right;
}

glm::vec3 Camera::getFront() const {
    return m_front;
}

glm::vec3 Camera::getPosition() const {
    return m_position;
}

void Camera::setPosition(glm::vec3 postion) {
    m_position = postion;
    updateMatrix();
}

glm::vec3 Camera::getDirection() const {
    return m_direction;
}

void Camera::setOrientation(float pitch, float yaw) {
    // Clamp pitch to avoid gimbal singularities at straight up/down.
    pitch = std::max(pitch, -89.0f);
    pitch = std::min(pitch, +89.0f);

    m_pitch = pitch;
    m_yaw = yaw;

    updateDirection();
    updateMatrix();
}

void Camera::updateDirection() {
    // Convert Euler angles to a normalized forward direction.
    float rpitch = glm::radians(m_pitch);
    float ryaw = glm::radians(m_yaw);

    m_direction.x = cos(ryaw) * cos(rpitch);
    m_direction.y = sin(rpitch);
    m_direction.z = sin(ryaw) * cos(rpitch);

    m_direction = glm::normalize(m_direction);

    m_right = glm::normalize(glm::cross(m_direction, VectorUp));
    m_front = -glm::normalize(glm::cross(m_right, VectorUp));
}

void Camera::updateOrientation() {
    
}

void Camera::updateMatrix() {
    // Cache view and combined projection-view matrices for render code.
    m_viewMatrix = glm::lookAt(m_position, m_position + m_direction, VectorUp);
    m_projectionViewMatrix = m_projectionMatrix * m_viewMatrix;
}

}