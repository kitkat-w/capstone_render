#include <fstream>
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include "tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// renderer.cpp
#include "renderer.hpp"
#include "shaders.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <map>

namespace MeshRenderer {

static GLuint vao = 0;
static std::map<int, GLuint> vbos;
static tinygltf::Model model;
static GLuint shaderProgram;
static GLuint MVP_u, sun_position_u, sun_color_u;
static glm::mat4 model_rot = glm::mat4(1.0f);
static glm::vec3 model_pos = glm::vec3(-3, 0, -3);

static bool loadModel(const std::string& filename) {
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) std::cout << "[GLTF Warning] " << warn << std::endl;
    if (!err.empty()) std::cerr << "[GLTF Error] " << err << std::endl;
    if (!res) std::cerr << "Failed to load glTF: " << filename << std::endl;
    return res;
}

static void bindMesh(tinygltf::Mesh& mesh) {
    for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const auto& bufferView = model.bufferViews[i];
        if (bufferView.target == 0) continue;

        const auto& buffer = model.buffers[bufferView.buffer];
        GLuint vbo;
        glGenBuffers(1, &vbo);
        vbos[i] = vbo;
        glBindBuffer(bufferView.target, vbo);
        glBufferData(bufferView.target, bufferView.byteLength,
                     &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
    }

    for (const auto& primitive : mesh.primitives) {
        for (const auto& attrib : primitive.attributes) {
            const auto& accessor = model.accessors[attrib.second];
            int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
            glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

            int size = accessor.type == TINYGLTF_TYPE_SCALAR ? 1 : accessor.type;
            int loc = attrib.first == "POSITION" ? 0 : attrib.first == "NORMAL" ? 1 : attrib.first == "TEXCOORD_0" ? 2 : -1;
            if (loc >= 0) {
                glEnableVertexAttribArray(loc);
                glVertexAttribPointer(loc, size, accessor.componentType,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      byteStride, BUFFER_OFFSET(accessor.byteOffset));
            }
        }

        if (!model.textures.empty()) {
            auto& tex = model.textures[0];
            if (tex.source >= 0) {
                auto& image = model.images[tex.source];
                GLuint texid;
                glGenTextures(1, &texid);
                glBindTexture(GL_TEXTURE_2D, texid);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                GLenum format = image.component == 1 ? GL_RED : image.component == 2 ? GL_RG : image.component == 3 ? GL_RGB : GL_RGBA;
                GLenum type = image.bits == 16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                             format, type, &image.image.at(0));
            }
        }
    }
}

static void bindModelNodes(tinygltf::Node& node) {
    if (node.mesh >= 0) bindMesh(model.meshes[node.mesh]);
    for (int child : node.children) bindModelNodes(model.nodes[child]);
}

static void bindModel() {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    const auto& scene = model.scenes[model.defaultScene];
    for (int node : scene.nodes) bindModelNodes(model.nodes[node]);

    glBindVertexArray(0);
    for (auto it = vbos.begin(); it != vbos.end();) {
        const auto& bufferView = model.bufferViews[it->first];
        if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
            glDeleteBuffers(1, &it->second);
            it = vbos.erase(it);
        } else {
            ++it;
        }
    }
}

void init(const std::string& pathToGLTF) {
    Shaders shader;
    shaderProgram = shader.pid;
    glUseProgram(shaderProgram);

    MVP_u = glGetUniformLocation(shaderProgram, "MVP");
    sun_position_u = glGetUniformLocation(shaderProgram, "sun_position");
    sun_color_u = glGetUniformLocation(shaderProgram, "sun_color");

    if (loadModel(pathToGLTF)) bindModel();
}

void render(int windowWidth, int windowHeight) {
    glUseProgram(shaderProgram);
    glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), model_pos) * model_rot;
    glm::mat4 view_mat = glm::lookAt(glm::vec3(2, 2, 20), model_pos, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)windowWidth / windowHeight, 0.01f, 1000.0f);
    glm::mat4 mvp = proj * view_mat * model_mat;

    glUniformMatrix4fv(MVP_u, 1, GL_FALSE, &mvp[0][0]);
    glUniform3fv(sun_position_u, 1, &glm::vec3(3.0f, 10.0f, -5.0f)[0]);
    glUniform3fv(sun_color_u, 1, &glm::vec3(1.0f, 1.0f, 1.0f)[0]);

    glBindVertexArray(vao);
    const auto& scene = model.scenes[model.defaultScene];
    for (int node : scene.nodes) {
        const auto& n = model.nodes[node];
        if (n.mesh >= 0) {
            const auto& mesh = model.meshes[n.mesh];
            for (const auto& prim : mesh.primitives) {
                const auto& indexAccessor = model.accessors[prim.indices];
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[indexAccessor.bufferView]);
                glDrawElements(prim.mode, indexAccessor.count, indexAccessor.componentType, BUFFER_OFFSET(indexAccessor.byteOffset));
            }
        }
    }
    glBindVertexArray(0);
}

void cleanup() {
    glDeleteVertexArrays(1, &vao);
    for (auto& [_, id] : vbos) glDeleteBuffers(1, &id);
    vbos.clear();
    glDeleteProgram(shaderProgram);
}

} // namespace MeshRenderer
