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
#include <vector>
#include <cstddef>

#include "Camera.h"
#include "Shader.h"
#include "DNA.h"
#include "Skeleton.h"
#include "CreatureMesh.h"

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

    std::vector<float> FlattenBoneEndpoints(const Skeleton& skeleton) {
        std::vector<float> data;
        data.reserve(skeleton.bones.size() * 2 * 3);
        for (const Bone& bone : skeleton.bones) {
            const glm::vec3& a = skeleton.joints[bone.startJoint];
            const glm::vec3& b = skeleton.joints[bone.endJoint];
            data.insert(data.end(), {a.x, a.y, a.z, b.x, b.y, b.z});
        }
        return data;
    }

    std::vector<float> FlattenJoints(const Skeleton& skeleton) {
        std::vector<float> data;
        data.reserve(skeleton.joints.size() * 3);
        for (const glm::vec3& p : skeleton.joints) {
            data.insert(data.end(), {p.x, p.y, p.z});
        }
        return data;
    }
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

    Shader lineShader(std::string(CREATURES_SHADER_DIR) + "line.vert",
                       std::string(CREATURES_SHADER_DIR) + "line.frag");
    Shader meshShader(std::string(CREATURES_SHADER_DIR) + "basic.vert",
                      std::string(CREATURES_SHADER_DIR) + "basic.frag");

    GLuint meshVao, meshVbo;
    glGenVertexArrays(1, &meshVao);
    glGenBuffers(1, &meshVbo);
    glBindVertexArray(meshVao);
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GLuint boneVao, boneVbo, jointVao, jointVbo;
    glGenVertexArrays(1, &boneVao);
    glGenBuffers(1, &boneVbo);
    glBindVertexArray(boneVao);
    glBindBuffer(GL_ARRAY_BUFFER, boneVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &jointVao);
    glGenBuffers(1, &jointVbo);
    glBindVertexArray(jointVao);
    glBindBuffer(GL_ARRAY_BUFFER, jointVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    uint32_t seedInput = 1;
    DNA currentDNA = GenerateDNA(seedInput);
    Skeleton currentSkeleton = BuildSkeleton(currentDNA);
    int boneVertexCount = 0;
    int jointVertexCount = 0;
    int meshVertexCount = 0;
    bool showSkeletonDebug = false;

    auto uploadSkeleton = [&](const Skeleton& skeleton) {
        std::vector<float> boneData = FlattenBoneEndpoints(skeleton);
        boneVertexCount = static_cast<int>(boneData.size() / 3);
        glBindBuffer(GL_ARRAY_BUFFER, boneVbo);
        glBufferData(GL_ARRAY_BUFFER, boneData.size() * sizeof(float), boneData.data(), GL_DYNAMIC_DRAW);

        std::vector<float> jointData = FlattenJoints(skeleton);
        jointVertexCount = static_cast<int>(jointData.size() / 3);
        glBindBuffer(GL_ARRAY_BUFFER, jointVbo);
        glBufferData(GL_ARRAY_BUFFER, jointData.size() * sizeof(float), jointData.data(), GL_DYNAMIC_DRAW);
    };
    uploadSkeleton(currentSkeleton);

    auto uploadMesh = [&](const Skeleton& skeleton, const DNA& dna) {
        std::vector<MeshVertex> meshData = BuildCreatureMesh(skeleton, dna);
        meshVertexCount = static_cast<int>(meshData.size());
        glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
        glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(MeshVertex), meshData.data(), GL_DYNAMIC_DRAW);
    };
    uploadMesh(currentSkeleton, currentDNA);

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
            currentSkeleton = BuildSkeleton(currentDNA);
            uploadSkeleton(currentSkeleton);
            uploadMesh(currentSkeleton, currentDNA);
        }
        ImGui::SameLine();
        if (ImGui::Button("Random seed")) {
            seedInput = std::random_device{}();
            currentDNA = GenerateDNA(seedInput);
            currentSkeleton = BuildSkeleton(currentDNA);
            uploadSkeleton(currentSkeleton);
            uploadMesh(currentSkeleton, currentDNA);
        }
        ImGui::Checkbox("Show skeleton (debug)", &showSkeletonDebug);
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

        meshShader.Use();
        meshShader.SetMat4("uModel", glm::mat4(1.0f));
        meshShader.SetMat4("uView", camera.GetViewMatrix());
        meshShader.SetMat4("uProjection", camera.GetProjectionMatrix(aspect));
        meshShader.SetVec3("uColor", glm::vec3(0.6f, 0.75f, 0.4f));
        glBindVertexArray(meshVao);
        glDrawArrays(GL_TRIANGLES, 0, meshVertexCount);

        if (showSkeletonDebug) {
            // Debug overlay: draw on top of the solid mesh regardless of what's in front of it.
            glDisable(GL_DEPTH_TEST);

            lineShader.Use();
            lineShader.SetMat4("uModel", glm::mat4(1.0f));
            lineShader.SetMat4("uView", camera.GetViewMatrix());
            lineShader.SetMat4("uProjection", camera.GetProjectionMatrix(aspect));

            lineShader.SetVec3("uColor", glm::vec3(0.9f, 0.2f, 0.2f));
            glBindVertexArray(boneVao);
            glDrawArrays(GL_LINES, 0, boneVertexCount);

            lineShader.SetVec3("uColor", glm::vec3(1.0f, 0.85f, 0.2f));
            glPointSize(8.0f);
            glBindVertexArray(jointVao);
            glDrawArrays(GL_POINTS, 0, jointVertexCount);

            glEnable(GL_DEPTH_TEST);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &meshVao);
    glDeleteBuffers(1, &meshVbo);
    glDeleteVertexArrays(1, &boneVao);
    glDeleteBuffers(1, &boneVbo);
    glDeleteVertexArrays(1, &jointVao);
    glDeleteBuffers(1, &jointVbo);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
