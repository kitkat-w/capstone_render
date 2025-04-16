#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace MeshRenderer {
    void init(const std::string& pathToGLTF); // compile shader
    // bool loadModel(const std::string& path); // load + bind GLTF
    void render(glm::mat4 mvp, glm::vec3 sunPos, glm::vec3 sunColor);
    void cleanup();
    }