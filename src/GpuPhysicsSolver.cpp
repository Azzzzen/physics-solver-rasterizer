#include "GpuPhysicsSolver.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <GL/glew.h>

namespace {
constexpr float kBaseGravity = 9.81f;

bool isFiniteVec3(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
}  // namespace

GpuPhysicsSolver::GpuPhysicsSolver(std::size_t rows, std::size_t cols, float spacing)
    : m_rows(rows),
      m_cols(cols),
      m_spacing(spacing),
      m_mass(0.1f),
      m_stiffness(250.0f),
      m_damping(0.3f),
      m_springDamping(0.8f),
      m_maxSpeed(8.0f),
      m_maxStretchRatio(1.08f),
      m_groundY(-1.2f),
      m_gravity(0.0f, -kBaseGravity, 0.0f),
      m_wind(0.0f),
      m_draggedIndex(-1),
      m_dragRayT(0.0f),
      m_dragTarget(0.0f),
      m_computeProgram(0),
      m_posSsboA(0),
      m_posSsboB(0),
      m_velSsboA(0),
      m_velSsboB(0),
      m_fixedSsbo(0),
      m_pingPongFlip(false) {
    if (rows < 2 || cols < 2) {
        throw std::runtime_error("GpuPhysicsSolver requires rows and cols >= 2");
    }

    const std::string source = loadTextFile("shaders/cloth_step.comp");
    m_computeProgram = compileComputeProgram(source);

    glGenBuffers(1, &m_posSsboA);
    glGenBuffers(1, &m_posSsboB);
    glGenBuffers(1, &m_velSsboA);
    glGenBuffers(1, &m_velSsboB);
    glGenBuffers(1, &m_fixedSsbo);

    initializeGrid();
    pinConstraints();
    uploadInitialStateToGpu();
}

GpuPhysicsSolver::~GpuPhysicsSolver() {
    if (m_fixedSsbo != 0) {
        glDeleteBuffers(1, &m_fixedSsbo);
    }
    if (m_velSsboB != 0) {
        glDeleteBuffers(1, &m_velSsboB);
    }
    if (m_velSsboA != 0) {
        glDeleteBuffers(1, &m_velSsboA);
    }
    if (m_posSsboB != 0) {
        glDeleteBuffers(1, &m_posSsboB);
    }
    if (m_posSsboA != 0) {
        glDeleteBuffers(1, &m_posSsboA);
    }
    if (m_computeProgram != 0) {
        glDeleteProgram(m_computeProgram);
    }
}

void GpuPhysicsSolver::step(float dt) {
    if (dt <= 0.0f) {
        return;
    }

    const float clampedDt = std::min(dt, 1.0f / 30.0f);
    const float maxSubstep = 1.0f / 240.0f;
    const int substeps = std::max(1, static_cast<int>(std::ceil(clampedDt / maxSubstep)));
    const float h = clampedDt / static_cast<float>(substeps);

    for (int i = 0; i < substeps; ++i) {
        const unsigned int posIn = m_pingPongFlip ? m_posSsboB : m_posSsboA;
        const unsigned int velIn = m_pingPongFlip ? m_velSsboB : m_velSsboA;
        const unsigned int posOut = m_pingPongFlip ? m_posSsboA : m_posSsboB;
        const unsigned int velOut = m_pingPongFlip ? m_velSsboA : m_velSsboB;

        glUseProgram(m_computeProgram);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posIn);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velIn);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, posOut);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, velOut);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_fixedSsbo);

        glUniform1i(glGetUniformLocation(m_computeProgram, "uRows"), static_cast<int>(m_rows));
        glUniform1i(glGetUniformLocation(m_computeProgram, "uCols"), static_cast<int>(m_cols));
        glUniform1i(glGetUniformLocation(m_computeProgram, "uNumParticles"), static_cast<int>(m_rows * m_cols));
        glUniform1f(glGetUniformLocation(m_computeProgram, "uDt"), h);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uSpacing"), m_spacing);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uMass"), m_mass);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uStiffness"), m_stiffness);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uDamping"), m_damping);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uSpringDamping"), m_springDamping);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uMaxSpeed"), m_maxSpeed);
        glUniform1f(glGetUniformLocation(m_computeProgram, "uGroundY"), m_groundY);
        glUniform3f(glGetUniformLocation(m_computeProgram, "uGravity"), m_gravity.x, m_gravity.y, m_gravity.z);
        glUniform3f(glGetUniformLocation(m_computeProgram, "uWind"), m_wind.x, m_wind.y, m_wind.z);
        glUniform1i(glGetUniformLocation(m_computeProgram, "uDraggedIndex"), m_draggedIndex);
        glUniform3f(
            glGetUniformLocation(m_computeProgram, "uDragTarget"),
            m_dragTarget.x,
            m_dragTarget.y,
            m_dragTarget.z);

        const unsigned int numParticles = static_cast<unsigned int>(m_rows * m_cols);
        const unsigned int groups = (numParticles + 127u) / 128u;
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        m_pingPongFlip = !m_pingPongFlip;
    }

    readBackPositions();
    for (const glm::vec3& p : m_positionsCpu) {
        if (!isFiniteVec3(p)) {
            reset();
            return;
        }
    }
}

void GpuPhysicsSolver::reset() {
    m_stiffness = 250.0f;
    m_damping = 0.3f;
    m_gravity = glm::vec3(0.0f, -kBaseGravity, 0.0f);
    m_wind = glm::vec3(0.0f);
    m_draggedIndex = -1;
    m_dragRayT = 0.0f;
    m_dragTarget = glm::vec3(0.0f);
    m_pingPongFlip = false;

    initializeGrid();
    pinConstraints();
    uploadInitialStateToGpu();
}

const std::vector<glm::vec3>& GpuPhysicsSolver::getPositions() const {
    return m_positionsCpu;
}

float GpuPhysicsSolver::getStiffness() const {
    return m_stiffness;
}

float GpuPhysicsSolver::getDamping() const {
    return m_damping;
}

float GpuPhysicsSolver::getGravityScale() const {
    return -m_gravity.y / kBaseGravity;
}

float GpuPhysicsSolver::getWindStrength() const {
    return m_wind.x;
}

void GpuPhysicsSolver::setStiffness(float stiffness) {
    m_stiffness = std::clamp(stiffness, 20.0f, 1200.0f);
}

void GpuPhysicsSolver::setDamping(float damping) {
    m_damping = std::clamp(damping, 0.01f, 2.0f);
}

void GpuPhysicsSolver::setGravityScale(float gravityScale) {
    const float clamped = std::clamp(gravityScale, 0.0f, 3.0f);
    m_gravity = glm::vec3(0.0f, -kBaseGravity * clamped, 0.0f);
}

void GpuPhysicsSolver::setWindStrength(float windStrength) {
    const float clamped = std::clamp(windStrength, -8.0f, 8.0f);
    m_wind = glm::vec3(clamped, 0.0f, 0.0f);
}

bool GpuPhysicsSolver::beginDrag(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance) {
    if (glm::length(rayDir) <= 1e-6f) {
        return false;
    }

    int bestIndex = -1;
    float bestDist = maxDistance;
    float bestT = 0.0f;

    for (std::size_t i = 0; i < m_positionsCpu.size(); ++i) {
        if (m_fixedFlags[i] != 0) {
            continue;
        }

        const glm::vec3 toParticle = m_positionsCpu[i] - rayOrigin;
        const float t = glm::dot(toParticle, rayDir);
        if (t < 0.0f) {
            continue;
        }

        const glm::vec3 closest = rayOrigin + rayDir * t;
        const float dist = glm::length(m_positionsCpu[i] - closest);
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = static_cast<int>(i);
            bestT = t;
        }
    }

    if (bestIndex < 0) {
        return false;
    }

    m_draggedIndex = bestIndex;
    m_dragRayT = bestT;
    m_dragTarget = rayOrigin + rayDir * bestT;
    return true;
}

void GpuPhysicsSolver::updateDrag(const glm::vec3& worldTarget) {
    if (m_draggedIndex < 0) {
        return;
    }
    m_dragTarget = worldTarget;
}

void GpuPhysicsSolver::updateDragFromRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    if (m_draggedIndex < 0 || glm::length(rayDir) <= 1e-6f) {
        return;
    }
    m_dragTarget = rayOrigin + rayDir * m_dragRayT;
}

void GpuPhysicsSolver::endDrag() {
    m_draggedIndex = -1;
}

bool GpuPhysicsSolver::isDragging() const {
    return m_draggedIndex >= 0;
}

std::size_t GpuPhysicsSolver::index(std::size_t row, std::size_t col) const {
    return row * m_cols + col;
}

void GpuPhysicsSolver::initializeGrid() {
    m_positionsCpu.resize(m_rows * m_cols);
    m_fixedFlags.assign(m_rows * m_cols, 0);

    const float halfWidth = 0.5f * static_cast<float>(m_cols - 1) * m_spacing;
    const float halfHeight = 0.5f * static_cast<float>(m_rows - 1) * m_spacing;

    for (std::size_t r = 0; r < m_rows; ++r) {
        for (std::size_t c = 0; c < m_cols; ++c) {
            const float x = static_cast<float>(c) * m_spacing - halfWidth;
            const float z = static_cast<float>(r) * m_spacing - halfHeight;
            m_positionsCpu[index(r, c)] = glm::vec3(x, 2.35f, z);
        }
    }
}

void GpuPhysicsSolver::pinConstraints() {
    m_fixedFlags[index(0, 0)] = 1;
    m_fixedFlags[index(0, m_cols - 1)] = 1;
}

void GpuPhysicsSolver::uploadInitialStateToGpu() {
    std::vector<glm::vec4> pos4(m_positionsCpu.size(), glm::vec4(0.0f));
    std::vector<glm::vec4> vel4(m_positionsCpu.size(), glm::vec4(0.0f));
    for (std::size_t i = 0; i < m_positionsCpu.size(); ++i) {
        pos4[i] = glm::vec4(m_positionsCpu[i], 0.0f);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_posSsboA);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(pos4.size() * sizeof(glm::vec4)),
        pos4.data(),
        GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_posSsboB);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(pos4.size() * sizeof(glm::vec4)),
        pos4.data(),
        GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_velSsboA);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(vel4.size() * sizeof(glm::vec4)),
        vel4.data(),
        GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_velSsboB);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(vel4.size() * sizeof(glm::vec4)),
        vel4.data(),
        GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_fixedSsbo);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(m_fixedFlags.size() * sizeof(int)),
        m_fixedFlags.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GpuPhysicsSolver::readBackPositions() {
    const unsigned int currentPos = m_pingPongFlip ? m_posSsboB : m_posSsboA;

    std::vector<glm::vec4> pos4(m_positionsCpu.size(), glm::vec4(0.0f));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, currentPos);
    glGetBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        static_cast<GLsizeiptr>(pos4.size() * sizeof(glm::vec4)),
        pos4.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    for (std::size_t i = 0; i < m_positionsCpu.size(); ++i) {
        m_positionsCpu[i] = glm::vec3(pos4[i]);
    }
}

std::string GpuPhysicsSolver::loadTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int GpuPhysicsSolver::compileComputeProgram(const std::string& source) {
    const char* src = source.c_str();
    const unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(1, logLength)), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Compute shader compilation failed: " + log);
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        int logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(1, logLength)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("Compute program link failed: " + log);
    }

    return program;
}
