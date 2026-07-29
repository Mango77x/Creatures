#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <cmath>

#include "Camera.h"
#include "Shader.h"
#include "DNA.h"
#include "Skeleton.h"
#include "CreatureMesh.h"
#include "Animation.h"
#include "Gait.h"
#include "Terrain.h"
#include "Physics.h"

namespace {
    constexpr float kTerrainHalfSize = 6.0f;

    // Free orbital camera controls (Phase 9 revision — see Camera.h): left
    // mouse drag rotates, scroll zooms. Right mouse button is reserved for
    // click-and-hold-to-move steering, so the two never conflict.
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

    // Remembers which monitor/spot the window was on last time it closed —
    // GLFW/Windows only default to the primary monitor otherwise, since
    // nothing tracks "last position" unless the app saves it itself.
    constexpr const char* kWindowStateFile = "window_state.txt";

    bool LoadWindowPos(int& x, int& y) {
        std::ifstream in(kWindowStateFile);
        if (!in) return false;
        in >> x >> y;
        return static_cast<bool>(in);
    }

    void SaveWindowPos(GLFWwindow* window) {
        int x, y;
        glfwGetWindowPos(window, &x, &y);
        std::ofstream out(kWindowStateFile);
        out << x << " " << y;
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

    // Wraps to (-pi, pi] so turning always takes the shortest way around
    // instead of spinning the long way when crossing the +-pi seam.
    float WrapAngle(float angle) {
        angle = fmodf(angle + glm::pi<float>(), glm::two_pi<float>());
        if (angle < 0.0f) angle += glm::two_pi<float>();
        return angle - glm::pi<float>();
    }

    // Same helper Animation.cpp uses to bend the front hips — duplicated
    // rather than shared across a header for one three-line function.
    glm::vec3 RotateAroundY(const glm::vec3& point, const glm::vec3& pivot, float angle) {
        glm::vec3 rel = point - pivot;
        float c = cosf(angle);
        float s = sinf(angle);
        return pivot + glm::vec3(rel.x * c + rel.z * s, rel.y, -rel.x * s + rel.z * c);
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
    // Created hidden so the saved position (if any) can be applied before
    // the window ever appears on the primary monitor — avoids a visible
    // jump from primary to the remembered monitor on startup.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Creatures - Procedural Creature Lab", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    int savedWindowX, savedWindowY;
    if (LoadWindowPos(savedWindowX, savedWindowY)) {
        glfwSetWindowPos(window, savedWindowX, savedWindowY);
    }
    glfwShowWindow(window);

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
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, color));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Smooth, world-fixed terrain (never moves with the creature's body
    // transform) — a heightfield of deliberate hills/depressions built from
    // TerrainHeight, so per-leg "raycasting" is a direct height sample
    // rather than ray/mesh intersection. Finer resolution than the old
    // block terraces since curvature (not flat plateaus) is the point here.
    std::vector<MeshVertex> terrainData = BuildTerrainMesh(kTerrainHalfSize, 32);
    int terrainVertexCount = static_cast<int>(terrainData.size());
    GLuint terrainVao, terrainVbo;
    glGenVertexArrays(1, &terrainVao);
    glGenBuffers(1, &terrainVbo);
    glBindVertexArray(terrainVao);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVbo);
    glBufferData(GL_ARRAY_BUFFER, terrainData.size() * sizeof(MeshVertex), terrainData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, color));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Boundary walls, marking (and enforcing, via a position clamp below) the
    // edge of the world so steering toward the mouse can't walk off it.
    std::vector<MeshVertex> wallData = BuildBoundaryWalls(kTerrainHalfSize, -0.6f, 1.0f);
    int wallVertexCount = static_cast<int>(wallData.size());
    GLuint wallVao, wallVbo;
    glGenVertexArrays(1, &wallVao);
    glGenBuffers(1, &wallVbo);
    glBindVertexArray(wallVao);
    glBindBuffer(GL_ARRAY_BUFFER, wallVbo);
    glBufferData(GL_ARRAY_BUFFER, wallData.size() * sizeof(MeshVertex), wallData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, color));
    glEnableVertexAttribArray(2);
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

    // Phase 10, step 1: a standalone hanging-chain test for the new
    // particle+constraint solver (Physics.h), fully decoupled from the
    // creature — validates the core Verlet/relaxation math (does it stay
    // stable, settle naturally, swing right when the pinned end moves)
    // before any creature-specific physics code is written. Reuses the same
    // GL_LINES-of-raw-positions layout as the skeleton debug overlay above.
    GLuint ropeVao, ropeVbo;
    glGenVertexArrays(1, &ropeVao);
    glGenBuffers(1, &ropeVbo);
    glBindVertexArray(ropeVao);
    glBindBuffer(GL_ARRAY_BUFFER, ropeVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    constexpr int kRopeParticleCount = 6;
    constexpr float kRopeSegmentLength = 0.2f * kCreatureScale;
    const glm::vec3 kRopeAnchorBase(2.5f, 2.0f, 0.0f);
    PhysicsBody ropeTest;
    ropeTest.particles.resize(kRopeParticleCount);
    for (int i = 0; i < kRopeParticleCount; ++i) {
        glm::vec3 pos = kRopeAnchorBase - glm::vec3(0.0f, kRopeSegmentLength * i, 0.0f);
        ropeTest.particles[i].position = pos;
        ropeTest.particles[i].previousPosition = pos;
        ropeTest.particles[i].inverseMass = (i == 0) ? 0.0f : 1.0f; // first particle pinned
    }
    for (int i = 0; i < kRopeParticleCount - 1; ++i) {
        ropeTest.distanceConstraints.push_back({i, i + 1, kRopeSegmentLength});
    }
    bool showRopeTest = false;

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

    auto uploadMesh = [&](const Skeleton& skeleton, const DNA& dna, float breathScale) {
        std::vector<MeshVertex> meshData = BuildCreatureMesh(skeleton, dna, breathScale);
        meshVertexCount = static_cast<int>(meshData.size());
        glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
        glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(MeshVertex), meshData.data(), GL_DYNAMIC_DRAW);
    };
    uploadMesh(currentSkeleton, currentDNA, 0.0f);

    AnimationState animState;
    glm::vec3 lookAtTarget = currentSkeleton.joints[HeadTip] + glm::vec3(0.0f, 0.0f, 0.6f);
    float lastTime = static_cast<float>(glfwGetTime());

    GaitParams gaitParams;
    struct LegDescriptor {
        SkeletonJoint hip, knee, foot;
        float phaseOffset;
    };
    // Diagonal trot: front-left + back-right swing together, offset by half a
    // cycle from front-right + back-left.
    const LegDescriptor legs[4] = {
        {FrontLeftHip, FrontLeftKnee, FrontLeftFoot, 0.0f},
        {FrontRightHip, FrontRightKnee, FrontRightFoot, 0.5f},
        {BackLeftHip, BackLeftKnee, BackLeftFoot, 0.5f},
        {BackRightHip, BackRightKnee, BackRightFoot, 0.0f},
    };

    // Phase 10, Step 4: each leg is its own PhysicsBody (Hip pinned each
    // frame to wherever the spine's animated hip joint currently is, Knee,
    // Foot) instead of exact analytic 2-bone IK — same "pin to a live
    // external anchor" pattern Animation.cpp already uses for the tail/spine.
    // See DEVELOPMENT_PLAN.md for the design writeup.
    PhysicsBody legBodies[4];
    bool legPhysicsInitialized = false;
    // Fast: unlike the tail/spine (deliberately lagging), a foot needs to
    // track the gait cycle closely or stepping reads as mushy/late. Distance
    // constraints + the pole constraint below fully determine the knee from
    // hip+foot geometrically (same as the old law-of-cosines solve), so the
    // knee needs no muscle target of its own.
    constexpr float kFootMuscleRate = 30.0f;
    // Natural bend range: never fully straight (penguin-stiff) nor bent past
    // a deep crouch — see Skeleton.cpp's kStandCrouchFactor, whose rest pose
    // already sits around 110 degrees at the knee.
    constexpr float kKneeMinBendAngle = glm::radians(40.0f);
    constexpr float kKneeMaxBendAngle = glm::radians(170.0f);
    // High friction: a planted foot shouldn't skid, unlike the free-sliding
    // default GroundConstraint::friction is tuned for.
    constexpr float kLegGroundFriction = 0.85f;

    // Persistent movement state: the creature steers toward wherever the
    // mouse points on the ground, instead of following a closed-form path.
    glm::vec3 bodyPos(0.0f, 0.0f, 0.0f);
    float bodyYaw = 0.0f;
    float gaitTime = 0.0f; // only advances while actually moving, so legs don't march in place when idle
    constexpr float kWalkSpeed = 0.9f * kCreatureScale; // units/sec, scaled with the creature (Skeleton.h)
    constexpr float kWallMargin = 0.6f;
    constexpr float kStopDistance = 0.05f;
    // Caps how fast bodyYaw itself may change, independent of how fast the
    // mouse target direction jumps around. Without this, a target that spins
    // faster than the spine's lag chain (Animation.cpp) can catch up makes
    // the gap between chest and hips wind up without bound — a real animal
    // can leap backward, but it does that as a discrete jump/pivot, not by
    // smearing its spine through a continuous impossible rotation.
    constexpr float kMaxTurnRate = glm::radians(220.0f); // rad/sec

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
            animState = AnimationState{};
            lookAtTarget = currentSkeleton.joints[HeadTip] + glm::vec3(0.0f, 0.0f, 0.6f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Random seed")) {
            seedInput = std::random_device{}();
            currentDNA = GenerateDNA(seedInput);
            currentSkeleton = BuildSkeleton(currentDNA);
            animState = AnimationState{};
            lookAtTarget = currentSkeleton.joints[HeadTip] + glm::vec3(0.0f, 0.0f, 0.6f);
        }
        ImGui::Checkbox("Show skeleton (debug)", &showSkeletonDebug);
        ImGui::Checkbox("Show rope physics test (Phase 10, step 1)", &showRopeTest);
        ImGui::SliderInt("Pixel scale", &pixelScale, 1, 10);
        ImGui::Text("seed: %u", currentDNA.seed);
        ImGui::Separator();

        // Tabbed layout (Phase 9): the panel kept growing as more DNA fields
        // landed, to the point of needing constant scrolling. Split into
        // browser-style tabs instead — loosely mirrors RujiK's own
        // Body/Colr/Limb/Detl editor tabs (screenshots discussed in
        // CLAUDE.md), though our grouping follows this project's existing
        // section names rather than copying his exactly. Every slider still
        // writes straight into currentDNA/gaitParams/lookAtTarget and the
        // skeleton/mesh rebuild every frame below, so switching tabs doesn't
        // change any behavior — it's purely how the same controls are laid out.
        if (ImGui::BeginTabBar("DNATabs")) {
            if (ImGui::BeginTabItem("Body")) {
                ImGui::SliderFloat("bodyLength", &currentDNA.bodyLength, 0.5f, 2.0f);
                ImGui::SliderFloat("bodyHeight", &currentDNA.bodyHeight, 0.35f, 1.35f);
                ImGui::SliderFloat("neckLength", &currentDNA.neckLength, 0.15f, 1.8f);
                ImGui::SliderFloat("tailLength", &currentDNA.tailLength, 0.15f, 2.2f);
                ImGui::SliderFloat("bodyFat", &currentDNA.bodyFat, 0.0f, 1.0f);
                ImGui::SliderFloat("muscle", &currentDNA.muscle, 0.0f, 1.0f);
                ImGui::SliderFloat("aggressiveness", &currentDNA.aggressiveness, 0.0f, 1.0f); // not wired to any visual yet
                ImGui::SliderFloat("spineArch", &currentDNA.spineArch, -0.15f, 0.3f);
                ImGui::SliderFloat("legHeightBias", &currentDNA.legHeightBias, -0.25f, 0.25f);
                ImGui::SliderFloat("neckPitch", &currentDNA.neckPitch, 20.0f, 75.0f);
                ImGui::SliderFloat("tailPitch", &currentDNA.tailPitch, -10.0f, 45.0f);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Details")) {
                ImGui::SliderFloat("headSize", &currentDNA.headSize, 0.6f, 1.8f);
                ImGui::SliderFloat("headLength", &currentDNA.headLength, 0.5f, 2.0f);
                ImGui::SliderFloat("snoutTaper", &currentDNA.snoutTaper, 0.15f, 0.85f);
                ImGui::SliderFloat("hornSize", &currentDNA.hornSize, 0.0f, 0.6f);
                ImGui::SliderFloat("eyeSize", &currentDNA.eyeSize, 0.05f, 0.3f);
                ImGui::SliderFloat("earSize", &currentDNA.earSize, 0.05f, 0.4f);
                ImGui::Text("legCount: %d", currentDNA.legCount); // fixed quadruped, see CLAUDE.md
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Color")) {
                ImGui::SliderFloat("bodyHue", &currentDNA.bodyHue, 0.0f, 1.0f);
                ImGui::SliderFloat("accentHueShift", &currentDNA.accentHueShift, -0.45f, 0.45f);
                ImGui::SliderFloat("colorSaturation", &currentDNA.colorSaturation, 0.0f, 1.0f);
                ImGui::SliderFloat("colorValue", &currentDNA.colorValue, 0.0f, 1.0f);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Animation")) {
                ImGui::SliderFloat3("Look-at target", &lookAtTarget.x, -2.0f, 5.0f);
                ImGui::Separator();
                ImGui::TextUnformatted("Gait");
                ImGui::SliderFloat("Gait speed", &gaitParams.speed, 0.2f, 3.0f);
                ImGui::SliderFloat("Stride length", &gaitParams.strideLength, 0.1f, 1.0f);
                ImGui::SliderFloat("Lift height", &gaitParams.liftHeight, 0.02f, 0.4f);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        // Rebuilt every frame (cheap: no allocation beyond a small vector,
        // plain arithmetic) so any slider drag above reshapes the rest pose
        // immediately. The follow-the-leader animation state isn't reset
        // here (only Generate/Random seed do that) — it smoothly eases
        // toward the new rest pose instead of snapping, which reads as the
        // creature reshaping live rather than jump-cutting.
        currentSkeleton = BuildSkeleton(currentDNA);

        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        if (showRopeTest) {
            // Oscillate the pinned end so the rest of the chain visibly
            // swings/lags behind it, not just hangs static — the actual
            // thing this test needs to prove before any creature code
            // builds on the same solver.
            glm::vec3 anchor = kRopeAnchorBase + glm::vec3(sinf(currentTime * 1.5f) * 0.6f, 0.0f, 0.0f);
            ropeTest.particles[0].position = anchor;
            constexpr glm::vec3 kRopeGravity(0.0f, -9.8f * kCreatureScale, 0.0f);
            float clampedDt = std::min(dt, 1.0f / 30.0f); // avoid a huge first-frame/stall dt destabilizing the solver
            StepPhysics(ropeTest, clampedDt, kRopeGravity);
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        int wantedLowResWidth = std::max(1, width / pixelScale);
        int wantedLowResHeight = std::max(1, height / pixelScale);
        if (wantedLowResWidth != lowResWidth || wantedLowResHeight != lowResHeight) {
            recreateLowResTarget(wantedLowResWidth, wantedLowResHeight);
        }
        float aspect = lowResHeight > 0 ? static_cast<float>(lowResWidth) / static_cast<float>(lowResHeight) : 1.0f;

        // Steer toward wherever the mouse points on the ground, but only
        // while the right mouse button is held (and the cursor is actually
        // over the 3D view, not the ImGui panel or outside the window) —
        // click-and-hold-to-move instead of the creature endlessly chasing
        // the cursor. Unproject the cursor into a world-space ray and
        // intersect it with the y=0 plane (close enough given how small the
        // terrain's bumps are).
        float clampLimit = kTerrainHalfSize - kWallMargin;

        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        bool cursorInWindow = mouseX >= 0.0 && mouseX <= windowWidth && mouseY >= 0.0 && mouseY <= windowHeight;
        bool rightMouseHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        if (rightMouseHeld && cursorInWindow && !ImGui::GetIO().WantCaptureMouse) {
            float ndcX = (2.0f * static_cast<float>(mouseX) / static_cast<float>(windowWidth)) - 1.0f;
            float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY) / static_cast<float>(windowHeight));

            glm::mat4 invViewProj = glm::inverse(camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix());
            glm::vec4 nearP = invViewProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
            glm::vec4 farP = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
            nearP /= nearP.w;
            farP /= farP.w;
            glm::vec3 rayOrigin(nearP);
            glm::vec3 rayDir = glm::normalize(glm::vec3(farP) - glm::vec3(nearP));

            glm::vec3 mouseGroundTarget = bodyPos;
            if (fabsf(rayDir.y) > 1e-5f) {
                float t = -rayOrigin.y / rayDir.y;
                if (t > 0.0f) mouseGroundTarget = rayOrigin + rayDir * t;
            }
            mouseGroundTarget.x = glm::clamp(mouseGroundTarget.x, -clampLimit, clampLimit);
            mouseGroundTarget.z = glm::clamp(mouseGroundTarget.z, -clampLimit, clampLimit);

            glm::vec3 toTarget = mouseGroundTarget - bodyPos;
            toTarget.y = 0.0f;
            float distToTarget = glm::length(toTarget);
            if (distToTarget > kStopDistance) {
                glm::vec3 moveDir = toTarget / distToTarget;
                bodyPos += moveDir * std::min(kWalkSpeed * dt, distToTarget);
                float desiredYaw = atan2f(moveDir.x, moveDir.z); // local forward is (0,0,1)
                float yawStep = glm::clamp(WrapAngle(desiredYaw - bodyYaw), -kMaxTurnRate * dt, kMaxTurnRate * dt);
                bodyYaw += yawStep;
                gaitTime += dt;
            }
        }
        bodyPos.x = glm::clamp(bodyPos.x, -clampLimit, clampLimit);
        bodyPos.z = glm::clamp(bodyPos.z, -clampLimit, clampLimit);

        // Animation needs this frame's bodyYaw (just updated above) to know
        // how far the spine should bend — see Animation.cpp's rearYawLag.
        Skeleton animatedSkeleton = ApplyAnimation(animState, currentSkeleton, currentTime, dt, lookAtTarget, bodyYaw);

        // The rigid world transform below uses the LAGGED rear orientation,
        // not the immediate bodyYaw — the front (chest/neck/head/front hips)
        // already got bent to bodyYaw in local space by ApplyAnimation, so
        // composing lagged-global * bent-local correctly lands the front at
        // bodyYaw while the rear (pelvis/tail/back legs, never bent locally)
        // lands at the lagged orientation. Using bodyYaw here directly would
        // double-apply the turn to the front instead. Only ONE transform is
        // ever used for rendering (uModel below) — the bend lives entirely
        // in local joint positions, not in a second world transform.
        glm::mat4 yawOnly = glm::rotate(glm::mat4(1.0f), animState.rearYawLag, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 bodyTransformFlat = glm::translate(glm::mat4(1.0f), bodyPos) * yawOnly;

        const glm::vec3& pelvisLocal = currentSkeleton.joints[Pelvis];

        // Per-leg raycast: for a heightfield, a vertical ray hit is just the
        // height function sampled at that (x, z) — see Terrain.h.
        glm::vec3 worldFootXZ[4];
        float swingLift[4];
        float groundHeight[4];
        for (int i = 0; i < 4; ++i) {
            glm::vec3 restFootLocal = currentSkeleton.joints[legs[i].foot];
            // Front feet's gait cycle has to be measured from the SAME bent
            // reference the front hip uses (chestAngleLag, Animation.cpp) —
            // otherwise the hip swings with the chest bend but its foot
            // target stays where an unbent body would put it, so the IK
            // over-stretches trying to close a gap that isn't really there
            // (reads as the front legs "flying"/not planting on a sharp
            // turn). Rotating the rest reference here, before the single
            // shared bodyTransform below, keeps hip and target in the same
            // frame without needing a second world transform.
            if (i < 2) { // front legs first, see LegDescriptor legs[] below
                restFootLocal = RotateAroundY(restFootLocal, pelvisLocal, animState.chestAngleLag);
            }
            glm::vec3 localGait = ComputeFootTarget(restFootLocal, gaitTime, legs[i].phaseOffset, gaitParams);
            swingLift[i] = localGait.y;

            glm::vec4 worldXZ4 = bodyTransformFlat * glm::vec4(localGait.x, 0.0f, localGait.z, 1.0f);
            worldFootXZ[i] = glm::vec3(worldXZ4.x, 0.0f, worldXZ4.z);
            groundHeight[i] = TerrainHeight(worldXZ4.x, worldXZ4.z);
        }

        // Pelvis/spine adjustment: fit the body's height and pitch/roll to the
        // four raycast points instead of only the per-leg IK reaching down —
        // this is what keeps the creature from looking like it's standing on
        // a flat plane on top of a slope.
        float avgGroundHeight = (groundHeight[0] + groundHeight[1] + groundHeight[2] + groundHeight[3]) * 0.25f;
        float frontAvg = (groundHeight[0] + groundHeight[1]) * 0.5f; // FrontLeft, FrontRight
        float backAvg = (groundHeight[2] + groundHeight[3]) * 0.5f;  // BackLeft, BackRight
        float rightAvg = (groundHeight[1] + groundHeight[3]) * 0.5f; // FrontRight, BackRight
        float leftAvg = (groundHeight[0] + groundHeight[2]) * 0.5f;  // FrontLeft, BackLeft

        glm::vec3 frontHipMid = (currentSkeleton.joints[FrontLeftHip] + currentSkeleton.joints[FrontRightHip]) * 0.5f;
        glm::vec3 backHipMid = (currentSkeleton.joints[BackLeftHip] + currentSkeleton.joints[BackRightHip]) * 0.5f;
        glm::vec3 rightHipMid = (currentSkeleton.joints[FrontRightHip] + currentSkeleton.joints[BackRightHip]) * 0.5f;
        glm::vec3 leftHipMid = (currentSkeleton.joints[FrontLeftHip] + currentSkeleton.joints[BackLeftHip]) * 0.5f;
        float bodyLengthApprox = glm::max(0.1f, glm::length(frontHipMid - backHipMid));
        float bodyWidthApprox = glm::max(0.1f, glm::length(rightHipMid - leftHipMid));

        constexpr float kMaxTilt = 0.4f; // radians, clamps extreme slopes
        float pitch = glm::clamp(atan2f(backAvg - frontAvg, bodyLengthApprox), -kMaxTilt, kMaxTilt);
        float roll = glm::clamp(atan2f(rightAvg - leftAvg, bodyWidthApprox), -kMaxTilt, kMaxTilt);

        glm::mat4 bodyTransform = glm::translate(glm::mat4(1.0f), glm::vec3(bodyPos.x, avgGroundHeight, bodyPos.z)) *
                                  yawOnly *
                                  glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f)) *
                                  glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 invBodyTransform = glm::inverse(bodyTransform);

        // Solve each leg's physics against its own ground point (X/Z from the
        // flat transform's sample, Y from the terrain height + swing arc),
        // converted into the tilted body's local space — everything (hip
        // pin, foot muscle target, ground reference height) lives in that
        // same local frame, matching how the old analytic IK operated.
        // hipLocal already carries the front-hip bend from ApplyAnimation,
        // and worldFootXZ above already accounts for it too (see
        // restFootLocal), so ONE shared bodyTransform is correct here for
        // every leg.
        if (!legPhysicsInitialized) {
            for (int i = 0; i < 4; ++i) {
                glm::vec3 hipRest = currentSkeleton.joints[legs[i].hip];
                glm::vec3 kneeRest = currentSkeleton.joints[legs[i].knee];
                glm::vec3 footRest = currentSkeleton.joints[legs[i].foot];
                legBodies[i].particles = {
                    {hipRest, hipRest, 0.0f},
                    {kneeRest, kneeRest, 1.0f},
                    {footRest, footRest, 1.0f},
                };
                legBodies[i].distanceConstraints = {
                    {0, 1, glm::length(kneeRest - hipRest)},
                    {1, 2, glm::length(footRest - kneeRest)},
                };
                legBodies[i].angleConstraints = {
                    {0, 1, 2, kKneeMinBendAngle, kKneeMaxBendAngle},
                };
                // poleDir = local-forward, same fixed direction the old
                // SolveTwoBoneIK used — see PoleConstraint's comment in
                // Physics.h for how a reversed (bird-style) knee would flip
                // this per leg later.
                legBodies[i].poleConstraints = {
                    {0, 1, 2, glm::vec3(0.0f, 0.0f, 1.0f)},
                };
                legBodies[i].muscleTargets = {
                    {2, footRest, kFootMuscleRate},
                };
                legBodies[i].groundConstraints = {
                    {2, 0.0f, kLegGroundFriction},
                };
            }
            legPhysicsInitialized = true;
        }

        for (int i = 0; i < 4; ++i) {
            // Refresh rest lengths every frame — same reasoning as the tail/
            // spine: live DNA edits (leg proportions) must reshape the leg
            // immediately instead of fighting stale lengths from init.
            legBodies[i].distanceConstraints[0].restLength =
                glm::length(currentSkeleton.joints[legs[i].knee] - currentSkeleton.joints[legs[i].hip]);
            legBodies[i].distanceConstraints[1].restLength =
                glm::length(currentSkeleton.joints[legs[i].foot] - currentSkeleton.joints[legs[i].knee]);

            glm::vec3 worldTarget(worldFootXZ[i].x, groundHeight[i] + swingLift[i], worldFootXZ[i].z);
            glm::vec4 localTarget4 = invBodyTransform * glm::vec4(worldTarget, 1.0f);
            // Same conversion for the bare ground point (no swing lift) —
            // the terrain sample is a world-space height, so it needs to go
            // through the same tilted-local transform as everything else
            // the leg solves against.
            glm::vec4 groundLocal4 = invBodyTransform * glm::vec4(worldFootXZ[i].x, groundHeight[i], worldFootXZ[i].z, 1.0f);

            legBodies[i].particles[0].position = animatedSkeleton.joints[legs[i].hip]; // hip pin, kept explicit
            legBodies[i].muscleTargets[0].target = glm::vec3(localTarget4);
            legBodies[i].groundConstraints[0].groundHeight = groundLocal4.y;

            // No gravity: unlike the tail (passively hanging) or spine
            // (lightly drooping), a leg is actively held by its foot's
            // muscle pull the whole time — real legs don't sag mid-stride.
            StepPhysics(legBodies[i], dt, glm::vec3(0.0f));

            animatedSkeleton.joints[legs[i].knee] = legBodies[i].particles[1].position;
            animatedSkeleton.joints[legs[i].foot] = legBodies[i].particles[2].position;
        }

        uploadSkeleton(animatedSkeleton);
        uploadMesh(animatedSkeleton, currentDNA, animState.breathScale);

        // Pass 1: render the 3D scene at low resolution.
        glBindFramebuffer(GL_FRAMEBUFFER, lowResFbo);
        glViewport(0, 0, lowResWidth, lowResHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        meshShader.Use();
        meshShader.SetMat4("uView", camera.GetViewMatrix());
        meshShader.SetMat4("uProjection", camera.GetProjectionMatrix(aspect));

        meshShader.SetMat4("uModel", glm::mat4(1.0f));
        // Terrain's top/riser colors are baked per-vertex now (Terrain.cpp),
        // matching the same uColor*vColor scheme creatures use — white
        // leaves them unmodified.
        meshShader.SetVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
        glBindVertexArray(terrainVao);
        glDrawArrays(GL_TRIANGLES, 0, terrainVertexCount);

        meshShader.SetVec3("uColor", glm::vec3(0.45f, 0.36f, 0.3f));
        glBindVertexArray(wallVao);
        glDrawArrays(GL_TRIANGLES, 0, wallVertexCount);

        meshShader.SetMat4("uModel", bodyTransform);
        // The creature's real color now lives per-vertex (Phase 9's DNA
        // palette, see CreatureMesh.cpp's BoneColor) — uColor just tints,
        // so pass white to leave it unmodified.
        meshShader.SetVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
        glBindVertexArray(meshVao);
        glDrawArrays(GL_TRIANGLES, 0, meshVertexCount);

        if (showSkeletonDebug) {
            // Debug overlay: draw on top of the solid mesh regardless of what's in front of it.
            glDisable(GL_DEPTH_TEST);

            lineShader.Use();
            lineShader.SetMat4("uModel", bodyTransform);
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

        if (showRopeTest) {
            // Rope particle positions are already absolute world coordinates
            // (the solver doesn't know about the creature's bodyTransform at
            // all), so this draws with an identity model matrix, unlike the
            // skeleton debug overlay above.
            std::vector<float> ropeLineData;
            ropeLineData.reserve((ropeTest.particles.size() - 1) * 2 * 3);
            for (size_t i = 0; i + 1 < ropeTest.particles.size(); ++i) {
                const glm::vec3& a = ropeTest.particles[i].position;
                const glm::vec3& b = ropeTest.particles[i + 1].position;
                ropeLineData.insert(ropeLineData.end(), {a.x, a.y, a.z, b.x, b.y, b.z});
            }
            glBindBuffer(GL_ARRAY_BUFFER, ropeVbo);
            glBufferData(GL_ARRAY_BUFFER, ropeLineData.size() * sizeof(float), ropeLineData.data(), GL_DYNAMIC_DRAW);

            glDisable(GL_DEPTH_TEST);
            lineShader.Use();
            lineShader.SetMat4("uModel", glm::mat4(1.0f));
            lineShader.SetMat4("uView", camera.GetViewMatrix());
            lineShader.SetMat4("uProjection", camera.GetProjectionMatrix(aspect));
            lineShader.SetVec3("uColor", glm::vec3(0.3f, 0.9f, 1.0f));
            glBindVertexArray(ropeVao);
            glDrawArrays(GL_LINES, 0, static_cast<int>(ropeLineData.size() / 3));
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

    SaveWindowPos(window);

    glDeleteVertexArrays(1, &meshVao);
    glDeleteBuffers(1, &meshVbo);
    glDeleteVertexArrays(1, &terrainVao);
    glDeleteBuffers(1, &terrainVbo);
    glDeleteVertexArrays(1, &wallVao);
    glDeleteBuffers(1, &wallVbo);
    glDeleteVertexArrays(1, &boneVao);
    glDeleteBuffers(1, &boneVbo);
    glDeleteVertexArrays(1, &jointVao);
    glDeleteBuffers(1, &jointVbo);
    glDeleteVertexArrays(1, &ropeVao);
    glDeleteBuffers(1, &ropeVbo);
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
