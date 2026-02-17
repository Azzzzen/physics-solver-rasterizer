#include "Shader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <GL/glew.h>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) : m_programId(0) {
    const std::string vertexSource = readFile(vertexPath);
    const std::string fragmentSource = readFile(fragmentPath);

    const unsigned int vertexShader = compile(GL_VERTEX_SHADER, vertexSource);
    const unsigned int fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);

    m_programId = glCreateProgram();
    glAttachShader(m_programId, vertexShader);
    glAttachShader(m_programId, fragmentShader);
    glLinkProgram(m_programId);

    int success = 0;
    glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
    if (success == 0) {
        int logLength = 0;
        glGetProgramiv(m_programId, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(logLength));
        glGetProgramInfoLog(m_programId, logLength, nullptr, log.data());
        throw std::runtime_error("Shader link failed: " + std::string(log.data()));
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader() {
    if (m_programId != 0) {
        glDeleteProgram(m_programId);
    }
}

void Shader::use() const {
    glUseProgram(m_programId);
}

unsigned int Shader::id() const {
    return m_programId;
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    const int location = glGetUniformLocation(m_programId, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    const int location = glGetUniformLocation(m_programId, name.c_str());
    glUniform3fv(location, 1, &value[0]);
}

void Shader::setFloat(const std::string& name, float value) const {
    const int location = glGetUniformLocation(m_programId, name.c_str());
    glUniform1f(location, value);
}

void Shader::setInt(const std::string& name, int value) const {
    const int location = glGetUniformLocation(m_programId, name.c_str());
    glUniform1i(location, value);
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open shader file: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int Shader::compile(unsigned int type, const std::string& source) {
    const unsigned int shader = glCreateShader(type);
    const char* ptr = source.c_str();
    glShaderSource(shader, 1, &ptr, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(logLength));
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        throw std::runtime_error("Shader compile failed: " + std::string(log.data()));
    }

    return shader;
}
