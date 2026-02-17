#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

Camera::Camera(float aspectRatio, glm::vec3 position)
    : m_position(position),
      m_front(0.0f, 0.0f, -1.0f),
      m_up(0.0f, 1.0f, 0.0f),
      m_right(1.0f, 0.0f, 0.0f),
      m_worldUp(0.0f, 1.0f, 0.0f),
      m_yaw(-90.0f),
      m_pitch(-15.0f),
      m_fov(45.0f),
      m_aspectRatio(aspectRatio),
      m_mouseSensitivity(0.08f) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(m_fov), m_aspectRatio, 0.1f, 100.0f);
}

const glm::vec3& Camera::getPosition() const {
    return m_position;
}

void Camera::processKeyboard(GLFWwindow* window, float deltaTime) {
    const float speed = 3.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_position += m_front * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_position -= m_front * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_position -= m_right * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_position += m_right * speed;
    }
}

void Camera::processMouseMovement(float xOffset, float yOffset) {
    m_yaw += xOffset * m_mouseSensitivity;
    m_pitch += yOffset * m_mouseSensitivity;
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
    updateCameraVectors();
}

void Camera::processMouseScroll(float yOffset) {
    m_fov -= yOffset;
    m_fov = glm::clamp(m_fov, 20.0f, 70.0f);
}

void Camera::setAspectRatio(float aspectRatio) {
    m_aspectRatio = aspectRatio;
}

void Camera::updateCameraVectors() {
    const glm::vec3 front(
        cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
        sin(glm::radians(m_pitch)),
        sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)));
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
