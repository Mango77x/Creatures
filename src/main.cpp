#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glm/glm.hpp>

#include <iostream>
#include <string>
#include <random>

#include "Camera.h"
#include "Shader.h"
#include "DNA.h"

namespace {
    Camera* g_Camera = nullptr;
    bool g_Dragging = false;
    double g_LastMouseX = 0.0;
    double g_LastMouseY = 0.0;

    void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
        if (ImGui::GetIO().WantCaptureMouse) return;
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            g_Dragging = (action == GLFW_PRESS);
            glfwGetCursorPos(window, &g_LastMouseX, &g_LastMouseY);
        }
    }

    void CursorPosCallback(GLFWwindow* /*window*/, double x, double y) {
        if (g_Dragging && g_Camera) {
            g_Camera->ProcessMouseDrag(static_cast<float>(x - g_LastMouseX),
                                        static_cast<float>(y - g_LastMouseY));
        }
        g_LastMouseX = x;
        g_LastMouseY = y;
    }

    void ScrollCallback(GLFWwindow* /*window*/, double /*xOffset*/, double yOffset) {
        if (ImGui::GetIO().WantCaptureMouse) return;
        if (g_Camera) g_Camera->ProcessScroll(static_cast<float>(yOffset));
    }

    // Unit cube (half-extent 0.75), position + normal per vertex, 6 faces x 2 triangles.
    constexpr float kCubeVertices[] = {
        // back (z-)
        -0.75f, -0.75f, -0.75f,  0.0f,  0.0f, -1.0f,
         0.75f,  0.75f, -0.75f,  0.0f,  0.0f, -1.0f,
         0.75f, -0.75f, -0.75f,  0.0f,  0.0f, -1.0f,
         0.75f,  0.75f, -0.75f,  0.0f,  0.0f, -1.0f,
        -0.75f, -0.75f, -0.75f,  0.0f,  0.0f, -1.0f,
        -0.75f,  0.75f, -0.75f,  0.0f,  0.0f, -1.0f,
        // front (z+)
        -0.75f, -0.75f,  0.75f,  0.0f,  0.0f,  1.0f,
         0.75f, -0.75f,  0.75f,  0.0f,  0.0f,  1.0f,
         0.75f,  0.75f,  0.75f,  0.0f,  0.0f,  1.0f,
         0.75f,  0.75f,  0.75f,  0.0f,  0.0f,  1.0f,
        -0.75f,  0.75f,  0.75f,  0.0f,  0.0f,  1.0f,
        -0.75f, -0.75f,  0.75f,  0.0f,  0.0f,  1.0f,
        // left (x-)
        -0.75f,  0.75f,  0.75f, -1.0f,  0.0f,  0.0f,
        -0.75f,  0.75f, -0.75f, -1.0f,  0.0f,  0.0f,
        -0.75f, -0.75f, -0.75f, -1.0f,  0.0f,  0.0f,
        -0.75f, -0.75f, -0.75f, -1.0f,  0.0f,  0.0f,
        -0.75f, -0.75f,  0.75f, -1.0f,  0.0f,  0.0f,
        -0.75f,  0.75f,  0.75f, -1.0f,  0.0f,  0.0f,
        // right (x+)
         0.75f,  0.75f,  0.75f,  1.0f,  0.0f,  0.0f,
         0.75f, -0.75f, -0.75f,  1.0f,  0.0f,  0.0f,
         0.75f,  0.75f, -0.75f,  1.0f,  0.0f,  0.0f,
         0.75f, -0.75f, -0.75f,  1.0f,  0.0f,  0.0f,
         0.75f,  0.75f,  0.75f,  1.0f,  0.0f,  0.0f,
         0.75f, -0.75f,  0.75f,  1.0f,  0.0f,  0.0f,
        // bottom (y-)
        -0.75f, -0.75f, -0.75f,  0.0f, -1.0f,  0.0f,
         0.75f, -0.75f, -0.75f,  0.0f, -1.0f,  0.0f,
         0.75f, -0.75f,  0.75f,  0.0f, -1.0f,  0.0f,
         0.75f, -0.75f,  0.75f,  0.0f, -1.0f,  0.0f,
        -0.75f, -0.75f,  0.75f,  0.0f, -1.0f,  0.0f,
        -0.75f, -0.75f, -0.75f,  0.0f, -1.0f,  0.0f,
        // top (y+)
        -0.75f,  0.75f, -0.75f,  0.0f,  1.0f,  0.0f,
         0.75f,  0.75f,  0.75f,  0.0f,  1.0f,  0.0f,
         0.75f,  0.75f, -0.75f,  0.0f,  1.0f,  0.0f,
         0.75f,  0.75f,  0.75f,  0.0f,  1.0f,  0.0f,
        -0.75f,  0.75f, -0.75f,  0.0f,  1.0f,  0.0f,
        -0.75f,  0.75f,  0.75f,  0.0f,  1.0f,  0.0f,
    };
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Creatures - Procedural Creature Lab", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Camera camera;
    g_Camera = &camera;

    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Shader shader(std::string(CREATURES_SHADER_DIR) + "basic.vert",
                  std::string(CREATURES_SHADER_DIR) + "basic.frag");

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    uint32_t seedInput = 1;
    DNA currentDNA = GenerateDNA(seedInput);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Creature Lab");
        ImGui::TextUnformatted("DNA");
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &seedInput);
        if (ImGui::Button("Generate")) {
            currentDNA = GenerateDNA(seedInput);
        }
        ImGui::SameLine();
        if (ImGui::Button("Random seed")) {
            seedInput = std::random_device{}();
            currentDNA = GenerateDNA(seedInput);
        }
        ImGui::Separator();
        ImGui::Text("seed: %u", currentDNA.seed);
        ImGui::Text("bodyLength: %.3f", currentDNA.bodyLength);
        ImGui::Text("bodyHeight: %.3f", currentDNA.bodyHeight);
        ImGui::Text("neckLength: %.3f", currentDNA.neckLength);
        ImGui::Text("tailLength: %.3f", currentDNA.tailLength);
        ImGui::Text("legCount: %d", currentDNA.legCount);
        ImGui::Text("hornSize: %.3f", currentDNA.hornSize);
        ImGui::Text("eyeSize: %.3f", currentDNA.eyeSize);
        ImGui::Text("earSize: %.3f", currentDNA.earSize);
        ImGui::Text("bodyFat: %.3f", currentDNA.bodyFat);
        ImGui::Text("muscle: %.3f", currentDNA.muscle);
        ImGui::Text("aggressiveness: %.3f", currentDNA.aggressiveness);
        ImGui::End();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

        shader.Use();
        shader.SetMat4("uModel", glm::mat4(1.0f));
        shader.SetMat4("uView", camera.GetViewMatrix());
        shader.SetMat4("uProjection", camera.GetProjectionMatrix(aspect));
        shader.SetVec3("uColor", glm::vec3(0.6f, 0.75f, 0.4f));

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
