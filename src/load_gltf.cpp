#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION

#include "tiny_gltf.h"
#include "load_gltf.hpp"
#include <iostream>
#include <vector>
#include <glm/glm.hpp>

// Loads first mesh from .glb file and fills vertex/index/texture data
bool loadGLTFMesh(const std::string& filename,
                  std::vector<Vertex>& outVertices,
                  std::vector<uint32_t>& outIndices,
                  std::vector<unsigned char>& outTextureImage,
                  int& texWidth,
                  int& texHeight,
                  int& texChannels) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) std::cout << "Warn: " << warn << std::endl;
    if (!err.empty()) std::cerr << "Err: " << err << std::endl;
    if (!ret) {
        std::cerr << "Failed to load GLTF: " << filename << std::endl;
        return false;
    }

    if (model.meshes.empty()) {
        std::cerr << "No mesh found in GLTF file." << std::endl;
        return false;
    }

    const tinygltf::Mesh& mesh = model.meshes[0];
    const tinygltf::Primitive& primitive = mesh.primitives[0];

    // POSITION
    const float* positions = nullptr;
    const float* normals = nullptr;
    const float* texcoords = nullptr;
    size_t vertexCount = 0;

    {
        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        positions = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
        vertexCount = accessor.count;
    }

    if (primitive.attributes.count("NORMAL")) {
        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        normals = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
    }

    if (primitive.attributes.count("TEXCOORD_0")) {
        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        texcoords = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + view.byteOffset]);
    }

    outVertices.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        Vertex v;
        v.position = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
        v.normal   = normals ? glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]) : glm::vec3(0.0f);
        v.texcoord = texcoords ? glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]) : glm::vec2(0.0f);
        outVertices.push_back(v);
    }

    // INDICES
    {
        const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        const unsigned char* data = buffer.data.data() + accessor.byteOffset + view.byteOffset;

        for (size_t i = 0; i < accessor.count; ++i) {
            uint32_t index = 0;
            switch (accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    index = data[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    index = reinterpret_cast<const uint16_t*>(data)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    index = reinterpret_cast<const uint32_t*>(data)[i];
                    break;
                default:
                    std::cerr << "Unsupported index format." << std::endl;
                    return false;
            }
            outIndices.push_back(index);
        }
    }

    // TEXTURE
    if (!model.textures.empty()) {
        const auto& texture = model.textures[0];
        const auto& image = model.images[texture.source];
        outTextureImage = image.image;
        texWidth = image.width;
        texHeight = image.height;
        texChannels = image.component;
    }

    return true;
}
