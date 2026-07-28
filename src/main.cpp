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
#include <algorithm>

#include "Camera.h"
#include "Shader.h"
#include "DNA.h"
#include "Skeleton.h"
#include "CreatureMesh.h"

namespace {
    Camera* g_Camera = nullptr;

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
    Shader screenShader(std::string(CREATURES_SHADER_DIR) + "screen.vert",
                         std::string(CREATURES_SHADER_DIR) + "screen.frag");

    // Fullscreen quad (NDC position + UV) used to blit the low-res render target.
    constexpr float kQuadVertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
    };
    GLuint quadVao, quadVbo;
    glGenVertexArrays(1, &quadVao);
    glGenBuffers(1, &quadVbo);
    glBindVertexArray(quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Low-res offscreen target: the 3D scene renders here at a fraction of the
    // window's resolution, then gets blitted back with GL_NEAREST filtering —
    // that upscale is what actually produces the chunky pixel-art look.
    GLuint lowResFbo = 0, lowResColorTex = 0, lowResDepthRbo = 0;
    int lowResWidth = 0, lowResHeight = 0;
    int pixelScale = 4;

    auto recreateLowResTarget = [&](int width, int height) {
        if (lowResFbo != 0) {
            glDeleteFramebuffers(1, &lowResFbo);
            glDeleteTextures(1, &lowResColorTex);
            glDeleteRenderbuffers(1, &lowResDepthRbo);
        }

        lowResWidth = width;
        lowResHeight = height;

        glGenFramebuffers(1, &lowResFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, lowResFbo);

        glGenTextures(1, &lowResColorTex);
        glBindTexture(GL_TEXTURE_2D, lowResColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, lowResWidth, lowResHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lowResColorTex, 0);

        glGenRenderbuffers(1, &lowResDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, lowResDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, lowResWidth, lowResHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, lowResDepthRbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Low-res framebuffer is incomplete" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

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
        ImGui::SliderInt("Pixel scale", &pixelScale, 1, 10);
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

        int wantedLowResWidth = std::max(1, width / pixelScale);
        int wantedLowResHeight = std::max(1, height / pixelScale);
        if (wantedLowResWidth != lowResWidth || wantedLowResHeight != lowResHeight) {
            recreateLowResTarget(wantedLowResWidth, wantedLowResHeight);
        }

        float aspect = lowResHeight > 0 ? static_cast<float>(lowResWidth) / static_cast<float>(lowResHeight) : 1.0f;

        // Pass 1: render the 3D scene at low resolution.
        glBindFramebuffer(GL_FRAMEBUFFER, lowResFbo);
        glViewport(0, 0, lowResWidth, lowResHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
            glPointSize(3.0f);
            glBindVertexArray(jointVao);
            glDrawArrays(GL_POINTS, 0, jointVertexCount);
        }

        // Pass 2: blit the low-res image back at window size with nearest-neighbor
        // filtering — that upscale is what turns it into chunky pixel art.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        screenShader.Use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lowResColorTex);
        screenShader.SetInt("uScreenTexture", 0);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

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
    glDeleteVertexArrays(1, &quadVao);
    glDeleteBuffers(1, &quadVbo);
    glDeleteFramebuffers(1, &lowResFbo);
    glDeleteTextures(1, &lowResColorTex);
    glDeleteRenderbuffers(1, &lowResDepthRbo);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
