#include "ObjLoader.h"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

namespace {

struct ObjIndex {
    int v = 0;
    int vn = 0;
};

ObjIndex parseObjIndexToken(const std::string& token) {
    ObjIndex out;

    const std::size_t p1 = token.find('/');
    if (p1 == std::string::npos) {
        out.v = std::stoi(token);
        return out;
    }

    out.v = std::stoi(token.substr(0, p1));

    const std::size_t p2 = token.find('/', p1 + 1);
    if (p2 == std::string::npos) {
        return out;
    }

    if (p2 + 1 < token.size()) {
        out.vn = std::stoi(token.substr(p2 + 1));
    }

    return out;
}

int resolveIndex(int objIndex, int arraySize) {
    if (objIndex > 0) {
        return objIndex;
    }
    if (objIndex < 0) {
        return arraySize + objIndex;
    }
    throw std::runtime_error("OBJ index 0 is invalid");
}

}  // namespace

ObjMeshData ObjLoader::load(const std::string& path, float scale) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open OBJ file: " + path);
    }

    std::vector<glm::vec3> positions(1, glm::vec3(0.0f));
    std::vector<glm::vec3> normals(1, glm::vec3(0.0f));

    ObjMeshData out;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "v") {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            iss >> x >> y >> z;
            positions.emplace_back(x * scale, y * scale, z * scale);
            continue;
        }

        if (tag == "vn") {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            iss >> x >> y >> z;
            normals.emplace_back(glm::normalize(glm::vec3(x, y, z)));
            continue;
        }

        if (tag != "f") {
            continue;
        }

        std::vector<ObjIndex> face;
        std::string token;
        while (iss >> token) {
            face.push_back(parseObjIndexToken(token));
        }

        if (face.size() < 3) {
            continue;
        }

        for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            const std::array<ObjIndex, 3> tri = {face[0], face[i], face[i + 1]};

            std::array<glm::vec3, 3> triPos;
            for (int k = 0; k < 3; ++k) {
                const int pIndex = resolveIndex(tri[k].v, static_cast<int>(positions.size()));
                if (pIndex <= 0 || pIndex >= static_cast<int>(positions.size())) {
                    throw std::runtime_error("OBJ position index out of range");
                }
                triPos[k] = positions[static_cast<std::size_t>(pIndex)];
            }

            glm::vec3 faceNormal = glm::normalize(glm::cross(triPos[1] - triPos[0], triPos[2] - triPos[0]));
            if (glm::length(faceNormal) < 1e-6f) {
                faceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            for (int k = 0; k < 3; ++k) {
                glm::vec3 normal = faceNormal;
                if (tri[k].vn != 0) {
                    const int nIndex = resolveIndex(tri[k].vn, static_cast<int>(normals.size()));
                    if (nIndex > 0 && nIndex < static_cast<int>(normals.size())) {
                        normal = normals[static_cast<std::size_t>(nIndex)];
                    }
                }

                out.vertices.push_back(MeshVertex{triPos[k], normal});
                out.indices.push_back(static_cast<unsigned int>(out.indices.size()));
            }
        }
    }

    if (out.vertices.empty()) {
        throw std::runtime_error("OBJ contains no renderable faces: " + path);
    }

    return out;
}
