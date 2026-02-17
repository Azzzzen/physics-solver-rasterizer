#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

class Camera {
public:
    Camera(float aspectRatio, glm::vec3 position = glm::vec3(0.0f, 1.5f, 4.0f));

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    const glm::vec3& getPosition() const;

    void processKeyboard(GLFWwindow* window, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset);
    void processMouseScroll(float yOffset);
    void setAspectRatio(float aspectRatio);

private:
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    float m_yaw;
    float m_pitch;
    float m_fov;
    float m_aspectRatio;
    float m_mouseSensitivity;

    void updateCameraVectors();
};
