#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class GpuPhysicsSolver {
public:
    GpuPhysicsSolver(std::size_t rows, std::size_t cols, float spacing);
    ~GpuPhysicsSolver();

    GpuPhysicsSolver(const GpuPhysicsSolver&) = delete;
    GpuPhysicsSolver& operator=(const GpuPhysicsSolver&) = delete;

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
    struct GpuParticle {
        glm::vec4 pos;
        glm::vec4 vel;
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
    float m_groundY;
    glm::vec3 m_gravity;
    glm::vec3 m_wind;

    std::vector<glm::vec3> m_positionsCpu;
    std::vector<int> m_fixedFlags;

    int m_draggedIndex;
    float m_dragRayT;
    glm::vec3 m_dragTarget;

    unsigned int m_computeProgram;
    unsigned int m_posSsboA;
    unsigned int m_posSsboB;
    unsigned int m_velSsboA;
    unsigned int m_velSsboB;
    unsigned int m_fixedSsbo;
    bool m_pingPongFlip;

    std::size_t index(std::size_t row, std::size_t col) const;
    void initializeGrid();
    void pinConstraints();
    void uploadInitialStateToGpu();
    void readBackPositions();

    static std::string loadTextFile(const std::string& path);
    static unsigned int compileComputeProgram(const std::string& source);
};
