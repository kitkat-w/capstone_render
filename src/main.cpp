// // main.cpp
// #define GLFW_INCLUDE_NONE

// #include <GLFW/glfw3.h>
// #include <cstdlib>
// #include <fontconfig/fontconfig.h>
// #include <glad/glad.h>
// #include <glm/vec3.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <imgui.h>
// #include <imgui_impl_glfw.h>
// #include <imgui_impl_opengl3.h>
// #include <spdlog/spdlog.h>

// #include "depth_camera.hpp"
// #include "face_reconstruction.hpp"
// #include "renderer.hpp"

// namespace {
// const char *NAME = "UsARMirror";
// }

// namespace UsArMirror {
// std::optional<std::string> get_default_font() {
//     FcConfig *config = FcInitLoadConfigAndFonts();
//     FcPattern *pattern = FcPatternCreate();
//     FcObjectSet *object_set = FcObjectSetBuild(FC_FILE, nullptr);
//     FcFontSet *font_set = FcFontList(config, pattern, object_set);

//     std::string font_path;
//     if (font_set && font_set->nfont > 0) {
//         FcChar8 *file = nullptr;
//         if (FcPatternGetString(font_set->fonts[0], FC_FILE, 0, &file) == FcResultMatch) {
//             font_path = reinterpret_cast<const char *>(file);
//         } else {
//             return std::nullopt;
//         }
//     } else {
//         return std::nullopt;
//     }

//     FcFontSetDestroy(font_set);
//     FcObjectSetDestroy(object_set);
//     FcPatternDestroy(pattern);
//     FcConfigDestroy(config);

//     return font_path;
// }

// extern "C" int main(int argc, char *argv[]) {
//     auto state = std::make_shared<State>();
//     spdlog::info("Starting {}", NAME);

//     if (!glfwInit()) {
//         spdlog::error("Failed to initialize glfw");
//         return EXIT_FAILURE;
//     }

//     GLFWwindow *window = glfwCreateWindow(state->viewportWidth * state->viewportScaling,
//                                           state->viewportHeight * state->viewportScaling, NAME, nullptr, nullptr);
//     if (!window) {
//         glfwTerminate();
//         spdlog::error("Failed to create window");
//         return EXIT_FAILURE;
//     }
//     glfwMakeContextCurrent(window);

//     if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
//         spdlog::error("Failed to load glad");
//         return EXIT_FAILURE;
//     }

//     const GLubyte *glVersion = glGetString(GL_VERSION);
//     const GLubyte *glRenderer = glGetString(GL_RENDERER);
//     spdlog::info("GL_VERSION: {}", reinterpret_cast<const char *>(glVersion));
//     spdlog::info("GL_RENDERER: {}", reinterpret_cast<const char *>(glRenderer));

//     glEnable(GL_BLEND);
//     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//     glEnable(GL_DEPTH_TEST);
//     glDepthFunc(GL_LESS);
//     glClearColor(0, 0, 0, 0);

//     glfwSwapInterval(0);

//     IMGUI_CHECKVERSION();
//     ImGui::CreateContext();
//     ImGuiIO &io = ImGui::GetIO();
//     io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
//     io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
//     ImGui_ImplGlfw_InitForOpenGL(window, true);
//     ImGui_ImplOpenGL3_Init();

//     auto font_path_res = get_default_font();
//     if (font_path_res.has_value()) {
//         std::string font_path = font_path_res.value();
//         spdlog::debug("Using font: {}", font_path);
//         ImFontConfig font_config;
//         io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, &font_config);
//     } else {
//         spdlog::warn("Could not find a default font, using the ImGui default font.");
//     }

//     auto depthcameraInput = std::make_shared<DepthCameraInput>(state, 2);

//     MeshRenderer::init("models/Cube/Cube.gltf"); 

//     while (!glfwWindowShouldClose(window)) {
//         glfwPollEvents();

//         ImGui_ImplOpenGL3_NewFrame();
//         ImGui_ImplGlfw_NewFrame();
//         ImGui::NewFrame();

//         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//         glDisable(GL_DEPTH_TEST);
//         depthcameraInput->render();
//         glEnable(GL_DEPTH_TEST);

//         glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0, -3));
//         // glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glfwGetTime(), glm::vec3(0, 1, 0));
//         // model *= rotation;

//         glm::mat4 view = glm::lookAt(glm::vec3(2, 2, 20), glm::vec3(-3, 0, -3), glm::vec3(0, 1, 0));
//         glm::mat4 projection = glm::perspective(glm::radians(45.0f), state->viewportWidth / (float)state->viewportHeight, 0.1f, 100.0f);
//         glm::mat4 mvp = projection * view * model;
//         glm::vec3 sunPos = glm::vec3(3.0, 10.0, -5.0);
//         glm::vec3 sunColor = glm::vec3(1.0);

//         glEnable(GL_BLEND);
//         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//         glDepthMask(GL_FALSE);

//         MeshRenderer::render(mvp, sunPos, sunColor);

//         glDepthMask(GL_TRUE);
//         glDisable(GL_BLEND);

//         ImGui::Render();
//         ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//         glfwSwapBuffers(window);
//     }

//     spdlog::info("Cleaning up...");
//     MeshRenderer::cleanup();
//     ImGui_ImplOpenGL3_Shutdown();
//     ImGui_ImplGlfw_Shutdown();
//     ImGui::DestroyContext();
//     glfwTerminate();
//     return EXIT_SUCCESS;
// }

// } // namespace UsArMirror

#include <fstream>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <opencv2/opencv.hpp>

#include "shaders.h"
#include "window.h"
#include "background_shader.h"
#include "depth_camera.hpp"
#include "common.hpp"
#include "face_reconstruction.hpp"
#include "second_cam.hpp"

// #include <imgui.h>
// #include <imgui_impl_glfw.h>
// #include <imgui_impl_opengl3.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include "tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

GLuint modelTexture = 0; // Global model texture

bool loadModel(tinygltf::Model &model, const char *filename) {
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
  if (!warn.empty()) {
    std::cout << "WARN: " << warn << std::endl;
  }

  if (!err.empty()) {
    std::cout << "ERR: " << err << std::endl;
  }

  if (!res)
    std::cout << "Failed to load glTF: " << filename << std::endl;
  else
    std::cout << "Loaded glTF: " << filename << std::endl;

  return res;
}

void bindMesh(std::map<int, GLuint>& vbos,
              tinygltf::Model &model, tinygltf::Mesh &mesh) {
  for (size_t i = 0; i < model.bufferViews.size(); ++i) {
    const tinygltf::BufferView &bufferView = model.bufferViews[i];
    if (bufferView.target == 0) {  // TODO impl drawarrays
      std::cout << "WARN: bufferView.target is zero" << std::endl;
      continue;  // Unsupported bufferView.
                 /*
                   From spec2.0 readme:
                   https://github.com/KhronosGroup/glTF/tree/master/specification/2.0
                            ... drawArrays function should be used with a count equal to
                   the count            property of any of the accessors referenced by the
                   attributes            property            (they are all equal for a given
                   primitive).
                 */
    }

    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
    std::cout << "bufferview.target " << bufferView.target << std::endl;

    GLuint vbo;
    glGenBuffers(1, &vbo);
    vbos[i] = vbo;
    glBindBuffer(bufferView.target, vbo);

    std::cout << "buffer.data.size = " << buffer.data.size()
              << ", bufferview.byteOffset = " << bufferView.byteOffset
              << std::endl;

    glBufferData(bufferView.target, bufferView.byteLength,
                 &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
  }

  for (size_t i = 0; i < mesh.primitives.size(); ++i) {
    tinygltf::Primitive primitive = mesh.primitives[i];
    tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

    for (auto &attrib : primitive.attributes) {
      tinygltf::Accessor accessor = model.accessors[attrib.second];
      int byteStride =
          accessor.ByteStride(model.bufferViews[accessor.bufferView]);
      glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

      int size = 1;
      if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        size = accessor.type;
      }
      int vaa = -1;
      if (attrib.first.compare("POSITION") == 0) vaa = 0;
      if (attrib.first.compare("NORMAL") == 0) vaa = 1;
      if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
      if (vaa > -1) {
        glEnableVertexAttribArray(vaa);
        glVertexAttribPointer(vaa, size, accessor.componentType,
                              accessor.normalized ? GL_TRUE : GL_FALSE,
                              byteStride, BUFFER_OFFSET(accessor.byteOffset));
      } else
        std::cout << "vaa missing: " << attrib.first << std::endl;
    }

    if (model.textures.size() > 0) {
      // fixme: Use material's baseColor
      glActiveTexture(GL_TEXTURE0);
      tinygltf::Texture &tex = model.textures[0];

      if (tex.source > -1) {

        GLuint texid;
        glGenTextures(1, &texid);

        tinygltf::Image &image = model.images[tex.source];

        glBindTexture(GL_TEXTURE_2D, texid);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        GLenum format = GL_RGBA;

        if (image.component == 1) {
          format = GL_RED;
        } else if (image.component == 2) {
          format = GL_RG;
        } else if (image.component == 3) {
          format = GL_RGB;
        } else {
          // ???
        }

        GLenum type = GL_UNSIGNED_BYTE;
        if (image.bits == 8) {
          // ok
        } else if (image.bits == 16) {
          type = GL_UNSIGNED_SHORT;
        } else {
          // ???
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                     format, type, &image.image.at(0));
      }
    }
  }
}

// bind models
void bindModelNodes(std::map<int, GLuint>& vbos, tinygltf::Model &model,
                    tinygltf::Node &node) {
  if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
    bindMesh(vbos, model, model.meshes[node.mesh]);
  }

  for (size_t i = 0; i < node.children.size(); i++) {
    assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
    bindModelNodes(vbos, model, model.nodes[node.children[i]]);
  }
}

std::pair<GLuint, std::map<int, GLuint>> bindModel(tinygltf::Model &model) {
    std::map<int, GLuint> vbos;
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const auto &bufferView = model.bufferViews[i];
        if (bufferView.target == 0) continue;
        const auto &buffer = model.buffers[bufferView.buffer];
        GLuint vbo;
        glGenBuffers(1, &vbo);
        vbos[i] = vbo;
        glBindBuffer(bufferView.target, vbo);
        glBufferData(bufferView.target, bufferView.byteLength,
                     &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
    }

    for (const auto &mesh : model.meshes) {
        for (const auto &primitive : mesh.primitives) {
            for (const auto &attrib : primitive.attributes) {
                const auto &accessor = model.accessors[attrib.second];
                int stride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);
                int loc = (attrib.first == "POSITION") ? 0 : (attrib.first == "NORMAL") ? 1 : 2;
                glEnableVertexAttribArray(loc);
                glVertexAttribPointer(loc, accessor.type, accessor.componentType,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      stride, BUFFER_OFFSET(accessor.byteOffset));
            }
            if (!model.textures.empty()) {
                const auto &tex = model.textures[0];
                if (tex.source > -1) {
                    glGenTextures(1, &modelTexture);
                    const auto &image = model.images[tex.source];
                    glBindTexture(GL_TEXTURE_2D, modelTexture);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                
                // t = (image.component == 1) ? GL_RED : (image.component == 2) ? GL_RG : (image.component == 3) ? GL_RGB : GL_RGBA;
                //     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                //                  format, GL_UNSIGNED_BYTE, image.image.data());
                }
            }
        }
    }

    glBindVertexArray(0);
    return {vao, vbos};
}

void drawMesh(const std::map<int, GLuint>& vbos,
              tinygltf::Model &model, tinygltf::Mesh &mesh) {
  for (size_t i = 0; i < mesh.primitives.size(); ++i) {
    tinygltf::Primitive primitive = mesh.primitives[i];
    tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

    glDrawElements(primitive.mode, indexAccessor.count,
                   indexAccessor.componentType,
                   BUFFER_OFFSET(indexAccessor.byteOffset));
  }
}

// recursively draw node and children nodes of model
void drawModelNodes(const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos,
                    tinygltf::Model &model, tinygltf::Node &node) {
  if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
    drawMesh(vaoAndEbos.second, model, model.meshes[node.mesh]);
  }
  for (size_t i = 0; i < node.children.size(); i++) {
    drawModelNodes(vaoAndEbos, model, model.nodes[node.children[i]]);
  }
}

void drawModel(const std::pair<GLuint, std::map<int, GLuint>> &vaoAndEbos, tinygltf::Model &model) {
    glBindVertexArray(vaoAndEbos.first);
    for (const auto &mesh : model.meshes) {
        for (const auto &primitive : mesh.primitives) {
            const auto &accessor = model.accessors[primitive.indices];
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vaoAndEbos.second.at(accessor.bufferView));
            glDrawElements(primitive.mode, accessor.count, accessor.componentType,
                           BUFFER_OFFSET(accessor.byteOffset));
        }
    }
    glBindVertexArray(0);
}

void dbgModel(tinygltf::Model &model) {
  for (auto &mesh : model.meshes) {
    std::cout << "mesh : " << mesh.name << std::endl;
    for (auto &primitive : mesh.primitives) {
      const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
      std::cout << "indexaccessor: count " << indexAccessor.count
                << ", type " << indexAccessor.componentType << std::endl;

      if (primitive.material >= 0 && primitive.material < model.materials.size()) {
        tinygltf::Material &mat = model.materials[primitive.material];
        for (auto &mats : mat.values) {
          std::cout << "mat : " << mats.first.c_str() << std::endl;
        }
      }

      for (auto &image : model.images) {
        std::cout << "image name : " << image.uri << std::endl;
        std::cout << "  size : " << image.image.size() << std::endl;
        std::cout << "  w/h : " << image.width << "/" << image.height << std::endl;
      }

      std::cout << "indices : " << primitive.indices << std::endl;
      std::cout << "mode     : " << "(" << primitive.mode << ")" << std::endl;

      for (auto &attrib : primitive.attributes) {
        std::cout << "attribute : " << attrib.first.c_str() << std::endl;
      }

      // 👇 Print vertex positions:
      if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
        const auto &accessor = model.accessors[primitive.attributes.at("POSITION")];
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto &buffer = model.buffers[bufferView.buffer];

        const unsigned char* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t count = accessor.count;
        int stride = accessor.ByteStride(bufferView);
        if (stride == 0) stride = sizeof(float) * 3; // Default stride for vec3

        std::cout << "Vertex positions:" << std::endl;
        for (size_t i = 0; i < count; ++i) {
          const float* pos = reinterpret_cast<const float*>(dataPtr + i * stride);
          std::cout << "  [" << i << "]: (" << pos[0] << ", " << pos[1] << ", " << pos[2] << ")" << std::endl;
        }
      }
    }
  }
}


glm::mat4 genView(glm::vec3 pos, glm::vec3 lookat) {
  // Camera matrix
  glm::mat4 view = glm::lookAt(
      pos,                // Camera in World Space
      lookat,             // and looks at the origin
      glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
  );

  return view;
}

glm::mat4 genMVP(glm::mat4 view_mat, glm::mat4 model_mat, float fov, int w,
                 int h) {
  glm::mat4 Projection =
      glm::perspective(glm::radians(fov), (float)w / (float)h, 0.01f, 1000.0f);

  // Or, for an ortho camera :
  // glm::mat4 Projection = glm::ortho(-10.0f,10.0f,-10.0f,10.0f,0.0f,100.0f);
  // // In world coordinates

  glm::mat4 mvp = Projection * view_mat * model_mat;

  return mvp;
}

void displayLoop(Window &window, const std::string &filename) {
  Shaders shader = Shaders();
  glUseProgram(shader.pid);

  // grab uniforms to modify
  GLuint MVP_u = glGetUniformLocation(shader.pid, "MVP");
  GLuint sun_position_u = glGetUniformLocation(shader.pid, "sun_position");
  GLuint sun_color_u = glGetUniformLocation(shader.pid, "sun_color");
  

  tinygltf::Model model;
  if (!loadModel(model, filename.c_str())) return;

  std::pair<GLuint, std::map<int, GLuint>> vaoAndEbos = bindModel(model);
  // dbgModel(model); return;

  // Model matrix : an identity matrix (model will be at the origin)
  glm::mat4 model_mat = glm::mat4(1.0f);
  glm::mat4 model_rot = glm::mat4(1.0f);
  glm::vec3 model_pos = glm::vec3(-3, 0, -3);

  // generate a camera view, based on eye-position and lookAt world-position
  glm::mat4 view_mat = genView(glm::vec3(2, 2, 20), model_pos);

  glm::vec3 sun_position = glm::vec3(3.0, 10.0, -5.0);
  glm::vec3 sun_color = glm::vec3(1.0);

  while (!window.Close()) {
    window.Resize();

    glClearColor(0.2, 0.2, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 trans =
        glm::translate(glm::mat4(1.0f), model_pos);  // reposition model
    model_rot = glm::rotate(model_rot, glm::radians(0.8f),
                            glm::vec3(0, 1, 0));  // rotate model on y axis
    model_mat = trans * model_rot;

    // build a model-view-projection
    GLint w, h;
    glfwGetWindowSize(window.window, &w, &h);
    glm::mat4 mvp = genMVP(view_mat, model_mat, 45.0f, w, h);
    glUniformMatrix4fv(MVP_u, 1, GL_FALSE, &mvp[0][0]);

    glUniform3fv(sun_position_u, 1, &sun_position[0]);
    glUniform3fv(sun_color_u, 1, &sun_color[0]);

    drawModel(vaoAndEbos, model);
    glfwSwapBuffers(window.window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &vaoAndEbos.first);
}

static void error_callback(int error, const char *description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}

glm::mat4 getProjectionFromIntrinsics(const cv::Mat& K, int width, int height, float near = 0.01f, float far = 1000.0f) {
  float fx = K.at<float>(0, 0);
  float fy = K.at<float>(1, 1);
  float cx = K.at<float>(0, 2);
  float cy = K.at<float>(1, 2);

  glm::mat4 proj = glm::mat4(0.0f);
  proj[0][0] = 2.0f * fx / width;
  proj[1][1] = 2.0f * fy / height;
  proj[2][0] = 2.0f * (cx / width) - 1.0f;
  proj[2][1] = 2.0f * (cy / height) - 1.0f;
  proj[2][2] = -(far + near) / (far - near);
  proj[2][3] = -1.0f;
  proj[3][2] = -(2.0f * far * near) / (far - near);
  return proj;
}



int main(int argc, char **argv) {
    std::string filename = "models/Cube/Cube.gltf";
    if (argc > 1) filename = argv[1];

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window window(1280, 480, "GLTF Viewer with Video Background");
    auto width = 1280;
    auto height = 480;
    glfwMakeContextCurrent(window.window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    BackgroundShader background;
    GLuint backgroundTexture;
    GLuint secondaryTexture;
    glGenTextures(1, &secondaryTexture);
    glGenTextures(1, &backgroundTexture);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    // glDisable(GL_CULL_FACE);

    auto state = std::make_shared<UsArMirror::State>();
    state->viewportWidth = 640;
    state->viewportHeight = 480;

    auto depthCameraInput = std::make_shared<UsArMirror::DepthCameraInput>(state, 0);
    auto secondaryCam = std::make_shared<UsArMirror::CameraInput>(state, 0);
    // auto faceRecon = std::make_shared<UsArMirror::FaceReconstruction>("share/");

    Shaders shader;
    glUseProgram(shader.pid);
    GLuint MVP_u = glGetUniformLocation(shader.pid, "MVP");
    GLuint sun_position_u = glGetUniformLocation(shader.pid, "sun_position");
    GLuint sun_color_u = glGetUniformLocation(shader.pid, "sun_color");
    GLuint tex_u = glGetUniformLocation(shader.pid, "tex");
    glUniform1i(tex_u, 0);

    tinygltf::Model model;
    if (!loadModel(model, filename.c_str())) return -1;
    auto vaoAndEbos = bindModel(model);

    glm::mat4 model_mat(1.0f), model_rot(1.0f);
    glm::vec3 model_pos(-3, 0, -3), sun_position(3.0f, 10.0f, -5.0f), sun_color(1.0f);


    while (!window.Close()) {
        window.Resize();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
        cv::Mat color, secColor, stitchedImage;
        rs2::frame depth;

        if (secondaryCam->getFrame(secColor) && depthCameraInput->getFrame(color)) {
            // Resize to same height (optional)
            // if (secondaryColor.size() != depthColor.size()) {
            //     cv::resize(depthColor, depthColor, secondaryColor.size());
            // }
            cv::imwrite("depth.png", color);
            cv::imwrite("secondary.png", secColor);

            depth = depthCameraInput->getDepth();

        
            // Stitch side-by-side
            cv::hconcat(secColor, color, stitchedImage);
        
            // Convert BGR to RGB
            cv::cvtColor(stitchedImage, stitchedImage, cv::COLOR_BGR2RGB);
        
            // Upload as single OpenGL texture
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, backgroundTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, stitchedImage.cols, stitchedImage.rows, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, stitchedImage.data);
        
            // Render full window
            glDisable(GL_DEPTH_TEST);
            background.render(backgroundTexture, width, height);
            glEnable(GL_DEPTH_TEST);
        }
      
    

        // Draw 3D model
        glUseProgram(shader.pid);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, modelTexture);

        model_rot = glm::rotate(model_rot, glm::radians(0.8f), glm::vec3(0, 1, 0));
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), model_pos);
        model_mat = trans * model_rot;

        glm::mat4 view = glm::lookAt(glm::vec3(2, 2, 20), model_pos, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), w / (float)h, 0.01f, 1000.0f);
        glm::mat4 mvp = proj * view * model_mat;

        glUniformMatrix4fv(MVP_u, 1, GL_FALSE, &mvp[0][0]);
        glUniform3fv(sun_position_u, 1, &sun_position[0]);
        glUniform3fv(sun_color_u, 1, &sun_color[0]);

        GLuint opacityLoc = glGetUniformLocation(shader.pid, "opacity");
        glUniform1f(opacityLoc, 0.5f); // 0.0 = transparent, 1.0 = opaque


        drawModel(vaoAndEbos, model);
        glfwSwapBuffers(window.window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vaoAndEbos.first);
    glfwTerminate();
    return 0;
}
