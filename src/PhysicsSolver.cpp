#include "PhysicsSolver.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

PhysicsSolver::PhysicsSolver(std::size_t rows, std::size_t cols, float spacing)
    : m_rows(rows),
      m_cols(cols),
      m_spacing(spacing),
      m_mass(0.1f),
      m_stiffness(250.0f),
      m_damping(0.3f),
      m_springDamping(0.8f),
      m_maxSpeed(8.0f),
      m_maxStretchRatio(1.08f),
      m_gravity(0.0f, -9.81f, 0.0f),
      m_wind(0.0f, 0.0f, 0.0f),
      m_draggedIndex(-1),
      m_dragRayT(0.0f),
      m_dragTarget(0.0f) {
    if (rows < 2 || cols < 2) {
        throw std::runtime_error("PhysicsSolver requires rows and cols >= 2");
    }

    initializeGrid();
    initializeSprings();
    pinConstraints();
}

void PhysicsSolver::step(float dt) {
    if (dt <= 0.0f) {
        return;
    }
    const float clampedDt = std::min(dt, 1.0f / 30.0f);
    const float maxSubstep = 1.0f / 240.0f;
    const int substeps = std::max(1, static_cast<int>(std::ceil(clampedDt / maxSubstep)));
    const float h = clampedDt / static_cast<float>(substeps);

    for (int i = 0; i < substeps; ++i) {
        integrateSubstep(h);
        satisfyStrainConstraints();
    }

    for (std::size_t i = 0; i < m_positions.size(); ++i) {
        const glm::vec3& p = m_positions[i];
        const glm::vec3& v = m_velocities[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(v.x) ||
            !std::isfinite(v.y) || !std::isfinite(v.z)) {
            reset();
            return;
        }
    }
}

void PhysicsSolver::reset() {
    m_stiffness = 250.0f;
    m_damping = 0.3f;
    m_gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    m_wind = glm::vec3(0.0f, 0.0f, 0.0f);
    m_draggedIndex = -1;
    m_dragRayT = 0.0f;
    m_dragTarget = glm::vec3(0.0f);
    initializeGrid();
    initializeSprings();
    pinConstraints();
}

const std::vector<glm::vec3>& PhysicsSolver::getPositions() const {
    return m_positions;
}

float PhysicsSolver::getStiffness() const {
    return m_stiffness;
}

float PhysicsSolver::getDamping() const {
    return m_damping;
}

float PhysicsSolver::getGravityScale() const {
    return -m_gravity.y / 9.81f;
}

float PhysicsSolver::getWindStrength() const {
    return m_wind.x;
}

void PhysicsSolver::setStiffness(float stiffness) {
    m_stiffness = std::clamp(stiffness, 20.0f, 1200.0f);
}

void PhysicsSolver::setDamping(float damping) {
    m_damping = std::clamp(damping, 0.01f, 2.0f);
}

void PhysicsSolver::setGravityScale(float gravityScale) {
    const float clamped = std::clamp(gravityScale, 0.0f, 3.0f);
    m_gravity = glm::vec3(0.0f, -9.81f * clamped, 0.0f);
}

void PhysicsSolver::setWindStrength(float windStrength) {
    const float clamped = std::clamp(windStrength, -8.0f, 8.0f);
    m_wind = glm::vec3(clamped, 0.0f, 0.0f);
}

bool PhysicsSolver::beginDrag(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance) {
    if (glm::length(rayDir) <= 1e-6f) {
        return false;
    }

    int bestIndex = -1;
    float bestDist = maxDistance;
    float bestT = 0.0f;

    for (std::size_t i = 0; i < m_positions.size(); ++i) {
        if (m_fixed[i]) {
            continue;
        }

        const glm::vec3 toParticle = m_positions[i] - rayOrigin;
        const float t = glm::dot(toParticle, rayDir);
        if (t < 0.0f) {
            continue;
        }

        const glm::vec3 closest = rayOrigin + rayDir * t;
        const float dist = glm::length(m_positions[i] - closest);
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

void PhysicsSolver::updateDrag(const glm::vec3& worldTarget) {
    if (m_draggedIndex < 0) {
        return;
    }
    m_dragTarget = worldTarget;
}

void PhysicsSolver::updateDragFromRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    if (m_draggedIndex < 0 || glm::length(rayDir) <= 1e-6f) {
        return;
    }
    m_dragTarget = rayOrigin + rayDir * m_dragRayT;
}

void PhysicsSolver::endDrag() {
    m_draggedIndex = -1;
}

bool PhysicsSolver::isDragging() const {
    return m_draggedIndex >= 0;
}

std::size_t PhysicsSolver::index(std::size_t row, std::size_t col) const {
    return row * m_cols + col;
}

void PhysicsSolver::addSpring(std::size_t r0, std::size_t c0, std::size_t r1, std::size_t c1) {
    const std::size_t i0 = index(r0, c0);
    const std::size_t i1 = index(r1, c1);
    const float restLength = glm::length(m_positions[i0] - m_positions[i1]);
    m_springs.push_back(Spring{i0, i1, restLength});
}

void PhysicsSolver::initializeGrid() {
    m_positions.resize(m_rows * m_cols);
    m_velocities.assign(m_rows * m_cols, glm::vec3(0.0f));
    m_fixed.assign(m_rows * m_cols, false);

    const float halfWidth = 0.5f * static_cast<float>(m_cols - 1) * m_spacing;
    const float halfHeight = 0.5f * static_cast<float>(m_rows - 1) * m_spacing;

    for (std::size_t r = 0; r < m_rows; ++r) {
        for (std::size_t c = 0; c < m_cols; ++c) {
            const float x = static_cast<float>(c) * m_spacing - halfWidth;
            const float z = static_cast<float>(r) * m_spacing - halfHeight;
            m_positions[index(r, c)] = glm::vec3(x, 2.35f, z);
        }
    }
}

void PhysicsSolver::initializeSprings() {
    m_springs.clear();

    for (std::size_t r = 0; r < m_rows; ++r) {
        for (std::size_t c = 0; c < m_cols; ++c) {
            if (c + 1 < m_cols) {
                addSpring(r, c, r, c + 1);
            }
            if (r + 1 < m_rows) {
                addSpring(r, c, r + 1, c);
            }

            if (r + 1 < m_rows && c + 1 < m_cols) {
                addSpring(r, c, r + 1, c + 1);
            }
            if (r + 1 < m_rows && c >= 1) {
                addSpring(r, c, r + 1, c - 1);
            }

            if (c + 2 < m_cols) {
                addSpring(r, c, r, c + 2);
            }
            if (r + 2 < m_rows) {
                addSpring(r, c, r + 2, c);
            }
        }
    }
}

void PhysicsSolver::pinConstraints() {
    m_fixed[index(0, 0)] = true;
    m_fixed[index(0, m_cols - 1)] = true;
}

void PhysicsSolver::integrateSubstep(float dt) {
    std::vector<glm::vec3> forces(m_positions.size(), m_gravity * m_mass);

    for (const Spring& spring : m_springs) {
        const glm::vec3 delta = m_positions[spring.a] - m_positions[spring.b];
        const float length = glm::length(delta);
        if (length <= 1e-6f) {
            continue;
        }

        const glm::vec3 direction = delta / length;
        const float stretch = length - spring.restLength;
        const glm::vec3 relativeVelocity = m_velocities[spring.a] - m_velocities[spring.b];
        const float springDampingForce = glm::dot(relativeVelocity, direction) * m_springDamping;
        const glm::vec3 springForce = (-m_stiffness * stretch - springDampingForce) * direction;

        forces[spring.a] += springForce;
        forces[spring.b] -= springForce;
    }

    for (std::size_t i = 0; i < m_positions.size(); ++i) {
        if (m_fixed[i]) {
            m_velocities[i] = glm::vec3(0.0f);
            continue;
        }
        if (static_cast<int>(i) == m_draggedIndex) {
            m_positions[i] = m_dragTarget;
            m_velocities[i] = glm::vec3(0.0f);
            continue;
        }

        forces[i] += m_wind;
        forces[i] += -m_damping * m_velocities[i];

        const glm::vec3 acceleration = forces[i] / m_mass;
        m_velocities[i] += acceleration * dt;
        const float speed = glm::length(m_velocities[i]);
        if (speed > m_maxSpeed) {
            m_velocities[i] *= m_maxSpeed / speed;
        }

        m_positions[i] += m_velocities[i] * dt;

        if (m_positions[i].y < -1.2f) {
            m_positions[i].y = -1.2f;
            m_velocities[i].y *= -0.15f;
        }
    }
}

void PhysicsSolver::satisfyStrainConstraints() {
    for (const Spring& spring : m_springs) {
        const glm::vec3 delta = m_positions[spring.a] - m_positions[spring.b];
        const float length = glm::length(delta);
        if (length <= 1e-6f) {
            continue;
        }

        const float maxLength = spring.restLength * m_maxStretchRatio;
        if (length <= maxLength) {
            continue;
        }

        const glm::vec3 direction = delta / length;
        const float correctionMagnitude = length - maxLength;
        const glm::vec3 correction = correctionMagnitude * direction;

        const bool fixedA = m_fixed[spring.a];
        const bool fixedB = m_fixed[spring.b];
        const bool draggedA = static_cast<int>(spring.a) == m_draggedIndex;
        const bool draggedB = static_cast<int>(spring.b) == m_draggedIndex;
        const bool lockA = fixedA || draggedA;
        const bool lockB = fixedB || draggedB;

        if (!lockA && !lockB) {
            m_positions[spring.a] -= 0.5f * correction;
            m_positions[spring.b] += 0.5f * correction;
        } else if (!lockA && lockB) {
            m_positions[spring.a] -= correction;
        } else if (lockA && !lockB) {
            m_positions[spring.b] += correction;
        }
    }

    if (m_draggedIndex >= 0) {
        m_positions[static_cast<std::size_t>(m_draggedIndex)] = m_dragTarget;
        m_velocities[static_cast<std::size_t>(m_draggedIndex)] = glm::vec3(0.0f);
    }
}
