#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "Camera.h"
#include "Mesh.h"
#include "PhysicsSolver.h"
#include "Shader.h"

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kShadowWidth = 3072;
constexpr int kShadowHeight = 3072;

struct AppContext {
    Camera* camera = nullptr;
    bool firstMouseSample = true;
    double lastX = 0.0;
    double lastY = 0.0;
};

struct SceneObject {
    std::shared_ptr<Mesh> mesh;
    glm::mat4 model;
    glm::vec3 color;
    float specularStrength;
    float shininess;
};

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

bool uiCapturingMouse() {
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

void frameBufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    if (width <= 0 || height <= 0) {
        return;
    }

    auto* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
    if (ctx != nullptr && ctx->camera != nullptr) {
        ctx->camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr || ctx->camera == nullptr) {
        return;
    }

    if (uiCapturingMouse()) {
        ctx->firstMouseSample = true;
        return;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
        ctx->firstMouseSample = true;
        return;
    }

    if (ctx->firstMouseSample) {
        ctx->lastX = xpos;
        ctx->lastY = ypos;
        ctx->firstMouseSample = false;
        return;
    }

    const float xOffset = static_cast<float>(xpos - ctx->lastX);
    const float yOffset = static_cast<float>(ctx->lastY - ypos);
    ctx->lastX = xpos;
    ctx->lastY = ypos;

    ctx->camera->processMouseMovement(xOffset, yOffset);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;

    if (uiCapturingMouse()) {
        return;
    }

    auto* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
    if (ctx != nullptr && ctx->camera != nullptr) {
        ctx->camera->processMouseScroll(static_cast<float>(yoffset));
    }
}

std::vector<MeshVertex> buildUnitCubeVertices() {
    return {
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
    };
}

std::vector<unsigned int> buildUnitCubeIndices() {
    return {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };
}

glm::mat4 trs(const glm::vec3& t, const glm::vec3& s) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), t);
    model = glm::scale(model, s);
    return model;
}

Ray screenPointToRay(float mouseX, float mouseY, int fbWidth, int fbHeight, const Camera& camera) {
    const float x = (2.0f * mouseX) / static_cast<float>(fbWidth) - 1.0f;
    const float y = 1.0f - (2.0f * mouseY) / static_cast<float>(fbHeight);
    const glm::vec4 clipNear(x, y, -1.0f, 1.0f);
    const glm::vec4 clipFar(x, y, 1.0f, 1.0f);

    const glm::mat4 invVP = glm::inverse(camera.getProjectionMatrix() * camera.getViewMatrix());
    glm::vec4 worldNear = invVP * clipNear;
    glm::vec4 worldFar = invVP * clipFar;
    worldNear /= worldNear.w;
    worldFar /= worldFar.w;

    Ray ray;
    ray.origin = glm::vec3(worldNear);
    ray.direction = glm::normalize(glm::vec3(worldFar - worldNear));
    return ray;
}

void drawSceneDepth(const Shader& depthShader, const Mesh& clothMesh, const std::vector<SceneObject>& sceneObjects) {
    depthShader.setMat4("uModel", glm::mat4(1.0f));
    clothMesh.draw();

    for (const SceneObject& obj : sceneObjects) {
        depthShader.setMat4("uModel", obj.model);
        obj.mesh->draw();
    }
}

void drawSceneMain(
    const Shader& shader,
    const Mesh& clothMesh,
    const std::vector<SceneObject>& sceneObjects,
    const glm::vec3& clothColor,
    float clothSpec,
    float clothShininess) {
    shader.setMat4("uModel", glm::mat4(1.0f));
    shader.setVec3("uBaseColor", clothColor);
    shader.setFloat("uSpecularStrength", clothSpec);
    shader.setFloat("uShininess", clothShininess);
    clothMesh.draw();

    for (const SceneObject& obj : sceneObjects) {
        shader.setMat4("uModel", obj.model);
        shader.setVec3("uBaseColor", obj.color);
        shader.setFloat("uSpecularStrength", obj.specularStrength);
        shader.setFloat("uShininess", obj.shininess);
        obj.mesh->draw();
    }
}

}  // namespace

int main() {
    try {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Cloth Lab", nullptr, nullptr);
        if (window == nullptr) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error("Failed to initialize GLEW");
        }

        glEnable(GL_DEPTH_TEST);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        const std::size_t rows = 35;
        const std::size_t cols = 35;
        const float spacing = 0.05f;

        PhysicsSolver solver(rows, cols, spacing);
        Mesh clothMesh(rows, cols, solver.getPositions());

        Shader shadingShader("shaders/vertex.glsl", "shaders/fragment.glsl");
        Shader depthShader("shaders/shadow_depth_vertex.glsl", "shaders/shadow_depth_fragment.glsl");
        Camera camera(static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight));

        auto cubeMesh = std::make_shared<Mesh>(buildUnitCubeVertices(), buildUnitCubeIndices(), false);

        std::vector<SceneObject> sceneObjects;
        sceneObjects.push_back({cubeMesh, trs(glm::vec3(0.0f, -1.28f, 0.0f), glm::vec3(9.0f, 0.12f, 9.0f)), glm::vec3(0.55f, 0.56f, 0.58f), 0.18f, 10.0f});
        sceneObjects.push_back({cubeMesh, trs(glm::vec3(0.0f, 0.8f, -4.5f), glm::vec3(9.0f, 4.0f, 0.12f)), glm::vec3(0.70f, 0.71f, 0.74f), 0.10f, 8.0f});
        sceneObjects.push_back({cubeMesh, trs(glm::vec3(-4.5f, 0.8f, 0.0f), glm::vec3(0.12f, 4.0f, 9.0f)), glm::vec3(0.69f, 0.72f, 0.76f), 0.10f, 8.0f});
        sceneObjects.push_back({cubeMesh, trs(glm::vec3(0.0f, 2.48f, -0.85f), glm::vec3(2.15f, 0.06f, 0.06f)), glm::vec3(0.86f, 0.86f, 0.88f), 0.30f, 22.0f});

        unsigned int depthMapFbo = 0;
        unsigned int depthMap = 0;
        glGenFramebuffers(1, &depthMapFbo);
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT,
            kShadowWidth,
            kShadowHeight,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        AppContext appContext;
        appContext.camera = &camera;
        glfwSetWindowUserPointer(window, &appContext);
        glfwSetFramebufferSizeCallback(window, frameBufferSizeCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        frameBufferSizeCallback(window, fbWidth, fbHeight);

        bool paused = false;
        bool wireframe = false;
        bool showHud = true;
        bool pPressedLastFrame = false;
        bool f1PressedLastFrame = false;
        bool hPressedLastFrame = false;
        bool rPressedLastFrame = false;

        bool leftMouseHeld = false;

        float lastTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window)) {
            const float now = static_cast<float>(glfwGetTime());
            const float dt = glm::min(now - lastTime, 0.033f);
            lastTime = now;

            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            const bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
            if (pPressed && !pPressedLastFrame) {
                paused = !paused;
            }
            pPressedLastFrame = pPressed;

            const bool f1Pressed = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1Pressed && !f1PressedLastFrame) {
                wireframe = !wireframe;
                glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            }
            f1PressedLastFrame = f1Pressed;

            const bool hPressed = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
            if (hPressed && !hPressedLastFrame) {
                showHud = !showHud;
            }
            hPressedLastFrame = hPressed;

            const bool rPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
            if (rPressed && !rPressedLastFrame) {
                solver.reset();
                clothMesh.updatePositions(solver.getPositions());
            }
            rPressedLastFrame = rPressed;

            if (showHud) {
                ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(340.0f, 240.0f), ImGuiCond_Always);
                ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

                float stiffness = solver.getStiffness();
                float damping = solver.getDamping();
                float gravity = solver.getGravityScale();
                float wind = solver.getWindStrength();

                if (ImGui::SliderFloat("Stiffness", &stiffness, 20.0f, 1200.0f)) {
                    solver.setStiffness(stiffness);
                }
                if (ImGui::SliderFloat("Damping", &damping, 0.01f, 2.0f)) {
                    solver.setDamping(damping);
                }
                if (ImGui::SliderFloat("Gravity", &gravity, 0.0f, 3.0f)) {
                    solver.setGravityScale(gravity);
                }
                if (ImGui::SliderFloat("Wind", &wind, -8.0f, 8.0f)) {
                    solver.setWindStrength(wind);
                }

                ImGui::Separator();
                ImGui::Text("P: Pause  R: Reset  F1: Wireframe  H: Toggle UI");
                ImGui::Text("Right Mouse: Look Around");
                ImGui::Text("Left Mouse: Drag Cloth (outside UI)");
                ImGui::Text("State: %s", paused ? "Paused" : "Running");
                ImGui::End();
            }

            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            double mouseXWindow = 0.0;
            double mouseYWindow = 0.0;
            glfwGetCursorPos(window, &mouseXWindow, &mouseYWindow);

            int winWidth = 1;
            int winHeight = 1;
            glfwGetWindowSize(window, &winWidth, &winHeight);
            const float scaleX = static_cast<float>(fbWidth) / static_cast<float>(std::max(winWidth, 1));
            const float scaleY = static_cast<float>(fbHeight) / static_cast<float>(std::max(winHeight, 1));
            const float mouseX = static_cast<float>(mouseXWindow) * scaleX;
            const float mouseY = static_cast<float>(mouseYWindow) * scaleY;
            const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            const bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

            const bool mouseCapturedByUi = ImGui::GetIO().WantCaptureMouse;

            if (leftDown && !leftMouseHeld && !mouseCapturedByUi && !rightDown) {
                const Ray ray = screenPointToRay(mouseX, mouseY, fbWidth, fbHeight, camera);
                solver.beginDrag(ray.origin, ray.direction, 0.18f);
            }
            if (leftDown && solver.isDragging() && !mouseCapturedByUi) {
                const Ray ray = screenPointToRay(mouseX, mouseY, fbWidth, fbHeight, camera);
                solver.updateDragFromRay(ray.origin, ray.direction);
            }
            if ((!leftDown || mouseCapturedByUi) && solver.isDragging()) {
                solver.endDrag();
            }

            leftMouseHeld = leftDown;

            camera.processKeyboard(window, dt);
            if (!paused) {
                solver.step(dt);
            }
            clothMesh.updatePositions(solver.getPositions());

            const glm::vec3 sunlightDirection = glm::normalize(glm::vec3(-0.62f, -1.0f, -0.42f));
            const glm::vec3 lightDirForShading = -sunlightDirection;
            const glm::vec3 lightPos = -sunlightDirection * 8.0f;
            const glm::mat4 lightProjection = glm::ortho(-7.0f, 7.0f, -7.0f, 7.0f, 0.5f, 22.0f);
            const glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 lightSpace = lightProjection * lightView;

            glViewport(0, 0, kShadowWidth, kShadowHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);

            depthShader.use();
            depthShader.setMat4("uLightSpace", lightSpace);
            drawSceneDepth(depthShader, clothMesh, sceneObjects);

            glDisable(GL_POLYGON_OFFSET_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glViewport(0, 0, fbWidth, fbHeight);
            glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shadingShader.use();
            shadingShader.setMat4("uView", camera.getViewMatrix());
            shadingShader.setMat4("uProj", camera.getProjectionMatrix());
            shadingShader.setMat4("uLightSpace", lightSpace);
            shadingShader.setVec3("uCameraPos", camera.getPosition());
            shadingShader.setVec3("uLightDir", lightDirForShading);
            shadingShader.setVec3("uPointLightPos", glm::vec3(1.8f, 2.2f, 1.4f));
            shadingShader.setVec3("uPointLightColor", glm::vec3(1.0f, 0.88f, 0.72f));
            shadingShader.setFloat("uPointLightIntensity", 1.45f);
            shadingShader.setFloat("uAmbientStrength", 0.22f);
            shadingShader.setInt("uShadowMap", 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, depthMap);

            drawSceneMain(
                shadingShader,
                clothMesh,
                sceneObjects,
                glm::vec3(0.79f, 0.30f, 0.24f),
                0.36f,
                36.0f);

            ImGui::Render();
            if (wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }

            glfwSwapBuffers(window);
        }

        glDeleteTextures(1, &depthMap);
        glDeleteFramebuffers(1, &depthMapFbo);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
