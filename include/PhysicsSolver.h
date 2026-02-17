#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

class PhysicsSolver {
public:
    PhysicsSolver(std::size_t rows, std::size_t cols, float spacing);

    void step(float dt);
    void reset();

    const std::vector<glm::vec3>& getPositions() const;
    float getStiffness() const;
    float getDamping() const;
    float getGravityScale() const;
    float getWindStrength() const;

    void setStiffness(float stiffness);
    void setDamping(float damping);
    void setGravityScale(float gravityScale);
    void setWindStrength(float windStrength);
    bool beginDrag(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance);
    void updateDrag(const glm::vec3& worldTarget);
    void updateDragFromRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    void endDrag();
    bool isDragging() const;

private:
    struct Spring {
        std::size_t a;
        std::size_t b;
        float restLength;
    };

    std::size_t m_rows;
    std::size_t m_cols;
    float m_spacing;

    float m_mass;
    float m_stiffness;
    float m_damping;
    float m_springDamping;
    float m_maxSpeed;
    float m_maxStretchRatio;
    glm::vec3 m_gravity;
    glm::vec3 m_wind;

    std::vector<glm::vec3> m_positions;
    std::vector<glm::vec3> m_velocities;
    std::vector<bool> m_fixed;
    std::vector<Spring> m_springs;
    int m_draggedIndex;
    float m_dragRayT;
    glm::vec3 m_dragTarget;

    std::size_t index(std::size_t row, std::size_t col) const;
    void addSpring(std::size_t r0, std::size_t c0, std::size_t r1, std::size_t c1);
    void initializeGrid();
    void initializeSprings();
    void pinConstraints();
    void integrateSubstep(float dt);
    void satisfyStrainConstraints();
};
