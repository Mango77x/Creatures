#include "Shader.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace {
    std::string ReadFile(const std::string& path) {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint CompileStage(GLenum stage, const std::string& source, const std::string& debugName) {
        GLuint id = glCreateShader(stage);
        const char* src = source.c_str();
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);

        GLint success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(id, sizeof(log), nullptr, log);
            std::cerr << "Shader compile error (" << debugName << "): " << log << std::endl;
        }
        return id;
    }
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    GLuint vertex = CompileStage(GL_VERTEX_SHADER, ReadFile(vertexPath), vertexPath);
    GLuint fragment = CompileStage(GL_FRAGMENT_SHADER, ReadFile(fragmentPath), fragmentPath);

    m_ProgramId = glCreateProgram();
    glAttachShader(m_ProgramId, vertex);
    glAttachShader(m_ProgramId, fragment);
    glLinkProgram(m_ProgramId);

    GLint success;
    glGetProgramiv(m_ProgramId, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_ProgramId, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    glDeleteProgram(m_ProgramId);
}

void Shader::Use() const {
    glUseProgram(m_ProgramId);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) const {
    GLint location = glGetUniformLocation(m_ProgramId, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const {
    GLint location = glGetUniformLocation(m_ProgramId, name.c_str());
    glUniform3fv(location, 1, &value[0]);
}
