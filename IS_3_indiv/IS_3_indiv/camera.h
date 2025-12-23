#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    float pitch = 25.0f;
    float yaw = 180.0f;
    float distance = 1200.0f;

    // Переменные для управления мышью
    float lastMouseX = 640.0f;
    float lastMouseY = 360.0f;
    bool firstMouse = true;
    float mouseSensitivity = 0.1f;
    bool mouseEnabled = true;

    void ProcessMouseMovement(float xpos, float ypos) {
        if (!mouseEnabled || firstMouse) {
            lastMouseX = xpos;
            lastMouseY = ypos;
            firstMouse = false;
            return;
        }

        float xoffset = xpos - lastMouseX;
        float yoffset = lastMouseY - ypos; // Обратный порядок, так как координаты Y идут снизу вверх
        lastMouseX = xpos;
        lastMouseY = ypos;

        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw += xoffset;
        pitch += yoffset;

        // Ограничиваем угол наклона, чтобы камера не переворачивалась
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    void SetMousePosition(float x, float y) {
        lastMouseX = x;
        lastMouseY = y;
    }

    void ToggleMouseControl(bool enabled) {
        mouseEnabled = enabled;
        if (enabled) {
            firstMouse = true;
        }
    }

    glm::mat4 GetView(glm::vec3 target) {
        float p = glm::radians(pitch);
        float y = glm::radians(yaw);
        glm::vec3 pos;
        pos.x = target.x + distance * cos(p) * sin(y);
        pos.y = target.y + distance * sin(p);
        pos.z = target.z + distance * cos(p) * cos(y);
        return glm::lookAt(pos, target, glm::vec3(0, 1, 0));
    }

    glm::mat4 GetViewAim(glm::vec3 shipPos) {
        glm::vec3 camPos = shipPos + glm::vec3(0, 50, 0);
        float p = glm::radians(pitch);
        float y = glm::radians(yaw);
        glm::vec3 direction;
        direction.x = cos(p) * sin(y);
        direction.y = sin(p);
        direction.z = cos(p) * cos(y);
        return glm::lookAt(camPos, camPos + direction, glm::vec3(0, 1, 0));
    }

    glm::vec3 GetForward() {
        float p = glm::radians(pitch);
        float y = glm::radians(yaw);
        return glm::normalize(glm::vec3(cos(p) * sin(y), sin(p), cos(p) * cos(y)));
    }
};

#endif