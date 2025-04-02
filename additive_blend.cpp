// additive_blend.cpp (Jetson Orin Nano compatible with GLFW window for OpenGL ES)

#include <GLFW/glfw3.h>
#include <GLES2/gl2.h>
#include <eos/core/Mesh.hpp>
#include <eos/core/read_obj.hpp>
#include <eos/fitting/RenderingParameters.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

namespace {
GLuint program = 0;
GLuint vbo = 0, ebo = 0, tex = 0;
size_t index_count = 0;

std::string load_shader_source(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Failed to open shader file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) throw std::runtime_error("Shader compilation failed");
    return shader;
}

GLuint create_program_from_files(const std::string& vert_path, const std::string& frag_path) {
    std::string vert_src = load_shader_source(vert_path);
    std::string frag_src = load_shader_source(frag_path);
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src.c_str());
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src.c_str());
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "aPosition");
    glBindAttribLocation(prog, 1, "aTexCoord");
    glLinkProgram(prog);
    return prog;
}
}

void init_additive_blend() {
    program = create_program_from_files("shaders/mesh.vert", "shaders/mesh.frag");
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenTextures(1, &tex);
}

void upload_mesh_and_texture(const eos::core::Mesh& mesh, const cv::Mat& texture) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    bool has_texcoords = !mesh.tci.empty();
    for (size_t i = 0; i < mesh.tvi.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int vi = mesh.tvi[i][j];
            int ti = has_texcoords ? mesh.tci[i][j] : vi;

            if (vi >= mesh.vertices.size() || ti >= mesh.texcoords.size()) {
                std::cerr << "[ERROR] Invalid index at face " << i << ", vertex " << j
                          << ": vi=" << vi << ", ti=" << ti << std::endl;
                continue;
            }

            const auto& v = mesh.vertices[vi];
            const auto& t = mesh.texcoords[ti];

            vertices.push_back(v[0]);
            vertices.push_back(v[1]);
            vertices.push_back(v[2]);
            vertices.push_back(t[0]);
            vertices.push_back(1.0f - t[1]);

            indices.push_back(static_cast<unsigned int>(vertices.size() / 5 - 1));
        }
    }

    index_count = indices.size();
    if (index_count == 0 || vertices.empty()) {
        std::cerr << "[ERROR] No valid vertices/indices found in mesh!" << std::endl;
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    cv::Mat rgb_texture;
    cv::cvtColor(texture, rgb_texture, cv::COLOR_BGR2RGB);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb_texture.cols, rgb_texture.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_texture.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void render_additive_blend(const Eigen::Matrix4f& mvp, int screen_width, int screen_height) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(program);

    GLint mvp_loc = glGetUniformLocation(program, "uMVP");
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.data());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint tex_loc = glGetUniformLocation(program, "uTexture");
    glUniform1i(tex_loc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);

    glDisable(GL_BLEND);
}

void cleanup_additive_blend() {
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(program);
}

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(640, 480, "Additive Blend", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    eos::core::Mesh mesh = eos::core::read_obj("output/fitted_face.obj");
    std::cout << "Loaded mesh: vertices=" << mesh.vertices.size()
              << ", texcoords=" << mesh.texcoords.size()
              << ", tvi=" << mesh.tvi.size()
              << ", tci=" << mesh.tci.size() << std::endl;

    cv::Mat texture = cv::imread("texture_grid.png");
    if (texture.empty()) {
        std::cerr << "Failed to load texture." << std::endl;
        return 1;
    }

    Eigen::Matrix4f identity = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f mvp = identity * identity;

    init_additive_blend();
    upload_mesh_and_texture(mesh, texture);

    while (!glfwWindowShouldClose(window)) {
        glViewport(0, 0, 640, 480);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_additive_blend(mvp, 640, 480);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanup_additive_blend();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
