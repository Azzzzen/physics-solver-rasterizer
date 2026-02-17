#include "Mesh.h"

#include <stdexcept>
#include <utility>

#include <GL/glew.h>

Mesh::Mesh(std::size_t rows, std::size_t cols, const std::vector<glm::vec3>& positions)
    : m_rows(rows), m_cols(cols), m_dynamicPositions(true), m_vao(0), m_vbo(0), m_ebo(0) {
    if (rows * cols != positions.size()) {
        throw std::runtime_error("Mesh positions size mismatch with rows*cols");
    }

    m_vertices.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        m_vertices[i].position = positions[i];
        m_vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    buildIndexBuffer();
    recomputeNormals();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    uploadToGpu(false);
}

Mesh::Mesh(std::vector<MeshVertex> vertices, std::vector<unsigned int> indices, bool dynamicPositions)
    : m_rows(0),
      m_cols(0),
      m_vertices(std::move(vertices)),
      m_indices(std::move(indices)),
      m_dynamicPositions(dynamicPositions),
      m_vao(0),
      m_vbo(0),
      m_ebo(0) {
    if (m_vertices.empty() || m_indices.empty()) {
        throw std::runtime_error("Mesh vertices/indices must not be empty");
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    uploadToGpu(false);
}

Mesh::~Mesh() {
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void Mesh::updatePositions(const std::vector<glm::vec3>& positions) {
    if (!m_dynamicPositions) {
        throw std::runtime_error("updatePositions is only supported for dynamic meshes");
    }
    if (positions.size() != m_vertices.size()) {
        throw std::runtime_error("updatePositions size mismatch");
    }

    for (std::size_t i = 0; i < positions.size(); ++i) {
        m_vertices[i].position = positions[i];
    }

    recomputeNormals();
    uploadToGpu(true);
}

void Mesh::draw() const {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::buildIndexBuffer() {
    m_indices.clear();
    m_indices.reserve((m_rows - 1) * (m_cols - 1) * 6);

    for (std::size_t r = 0; r < m_rows - 1; ++r) {
        for (std::size_t c = 0; c < m_cols - 1; ++c) {
            const unsigned int i0 = static_cast<unsigned int>(r * m_cols + c);
            const unsigned int i1 = static_cast<unsigned int>(r * m_cols + c + 1);
            const unsigned int i2 = static_cast<unsigned int>((r + 1) * m_cols + c);
            const unsigned int i3 = static_cast<unsigned int>((r + 1) * m_cols + c + 1);

            m_indices.push_back(i0);
            m_indices.push_back(i2);
            m_indices.push_back(i1);

            m_indices.push_back(i1);
            m_indices.push_back(i2);
            m_indices.push_back(i3);
        }
    }
}

void Mesh::recomputeNormals() {
    for (auto& v : m_vertices) {
        v.normal = glm::vec3(0.0f);
    }

    for (std::size_t i = 0; i < m_indices.size(); i += 3) {
        MeshVertex& a = m_vertices[m_indices[i + 0]];
        MeshVertex& b = m_vertices[m_indices[i + 1]];
        MeshVertex& c = m_vertices[m_indices[i + 2]];

        const glm::vec3 e1 = b.position - a.position;
        const glm::vec3 e2 = c.position - a.position;
        const glm::vec3 n = glm::cross(e1, e2);

        a.normal += n;
        b.normal += n;
        c.normal += n;
    }

    for (auto& v : m_vertices) {
        const float len = glm::length(v.normal);
        if (len > 1e-6f) {
            v.normal /= len;
        } else {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

void Mesh::uploadToGpu(bool dynamicOnly) {
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(MeshVertex)),
        m_vertices.data(),
        m_dynamicPositions ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    if (!dynamicOnly) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)),
            m_indices.data(),
            GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(MeshVertex),
            reinterpret_cast<void*>(offsetof(MeshVertex, normal)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(0);
}
