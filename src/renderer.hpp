#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace MeshRenderer {

// struct Vertex {
//     glm::vec3 position;
//     glm::vec3 normal;
// };

// Initializes OpenGL resources and compiles shaders
void init();

// Loads a GLTF (.glb) model and uploads it to GPU buffers
bool loadModel(const std::string& filename);

// Renders the loaded model using the default view/projection/camera
void renderToTexture();

// Optional: retrieves rendered texture (FBO integration placeholder)
void renderToTexture();
void renderToTexture(const float* viewMatrix, const float* projMatrix);

// Releases OpenGL resources
void cleanup();

} // namespace MeshRenderer
