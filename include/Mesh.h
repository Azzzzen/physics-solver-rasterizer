#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

class Mesh {
public:
    Mesh(std::size_t rows, std::size_t cols, const std::vector<glm::vec3>& positions);
    Mesh(std::vector<MeshVertex> vertices, std::vector<unsigned int> indices, bool dynamicPositions = false);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void updatePositions(const std::vector<glm::vec3>& positions);
    void draw() const;

private:
    std::size_t m_rows;
    std::size_t m_cols;
    std::vector<MeshVertex> m_vertices;
    std::vector<unsigned int> m_indices;
    bool m_dynamicPositions;

    unsigned int m_vao;
    unsigned int m_vbo;
    unsigned int m_ebo;

    void buildIndexBuffer();
    void recomputeNormals();
    void uploadToGpu(bool dynamicOnly);
};
