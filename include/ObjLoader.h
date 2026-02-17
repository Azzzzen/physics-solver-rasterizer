#pragma once

#include <string>
#include <vector>

#include "Mesh.h"

struct ObjMeshData {
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
};

class ObjLoader {
public:
    static ObjMeshData load(const std::string& path, float scale = 1.0f);
};
