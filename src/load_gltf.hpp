#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

bool loadGLTFMesh(const std::string& filename,
                  std::vector<Vertex>& outVertices,
                  std::vector<uint32_t>& outIndices,
                  std::vector<unsigned char>& outTextureImage,
                  int& texWidth,
                  int& texHeight,
                  int& texChannels);
