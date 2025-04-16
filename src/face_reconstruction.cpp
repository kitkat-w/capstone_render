#include "face_reconstruction.hpp"

#include <eos/core/image/opencv_interop.hpp>
#include <eos/core/write_obj.hpp>
#include <eos/render/draw_utils.hpp>
#include <eos/render/texture_extraction.hpp>
#include <eos/fitting/fitting.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace UsArMirror {

using namespace eos;
using namespace std;

FaceReconstruction::FaceReconstruction(const std::string& model_path) {
    loadEOSModels(model_path);
}

void FaceReconstruction::loadEOSModels(const std::string& path) {

    detector = dlib::get_frontal_face_detector();
    try {
        dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> predictor;
    } catch (dlib::serialization_error& e) {
        throw std::runtime_error(std::string("Could not load landmark model: ") + e.what());
    }
    spdlog::info("Deserialized dlib predictor");

    morphablemodel::MorphableModel model = morphablemodel::load_model(path + "/sfm_shape_3448.bin");
    std::vector<morphablemodel::Blendshape> blendshapes = morphablemodel::load_blendshapes(path + "/expression_blendshapes_3448.bin");
    morphable_model_ = morphablemodel::MorphableModel(
        model.get_shape_model(), blendshapes,
        model.get_color_model(), cpp17::nullopt,
        model.get_texture_coordinates()
    );
    landmark_mapper_ = core::LandmarkMapper(path + "/ibug_to_sfm.txt");
    edge_topology_ = morphablemodel::load_edge_topology(path + "/sfm_3448_edge_topology.json");
    ibug_contour_ = fitting::ContourLandmarks::load(path + "/ibug_to_sfm.txt");
    model_contour_ = fitting::ModelContour::load(path + "/sfm_model_contours.json");
}

std::vector<Eigen::Vector3f> compute_vertex_normals(
    const std::vector<Eigen::Vector3f>& vertices,
    const std::vector<std::array<int, 3>>& triangles)
{
    std::vector<Eigen::Vector3f> normals(vertices.size(), Eigen::Vector3f::Zero());

    for (const auto& tri : triangles)
    {
        const Eigen::Vector3f& v0 = vertices[tri[0]];
        const Eigen::Vector3f& v1 = vertices[tri[1]];
        const Eigen::Vector3f& v2 = vertices[tri[2]];

        Eigen::Vector3f normal = (v1 - v0).cross(v2 - v0).normalized();

        normals[tri[0]] += normal;
        normals[tri[1]] += normal;
        normals[tri[2]] += normal;
    }

    for (auto& n : normals)
    {
        n.normalize();
    }

    return normals;
}


void FaceReconstruction::fitAndRender(
    const cv::Mat& colorImage,
    const rs2::depth_frame& depthFrame,
    const cv::Mat& cameraIntrinsics,         // 3x3
    const cv::Mat& extrinsicsDepthToColor,   // 4x4
    GLuint& vao,
    GLuint& vbo,
    GLuint& texture,
    glm::mat4& model_matrix,
    int width,
    int height,
    int& faceVertexCount)
{
    if (colorImage.empty() || !depthFrame) {
        std::cout << "no image" << std::endl;
        return;
    }

    // Step 1: Detect landmarks (Dlib)
    dlib::cv_image<dlib::bgr_pixel> dlib_img(colorImage);
    std::vector<dlib::rectangle> faces = detector(dlib_img);
    if (faces.empty()) {
        std::cout << "No face detected" << std::endl;
        return;
    }
    dlib::full_object_detection shape = predictor(dlib_img, faces[0]);
    spdlog::info("[fitAndRender] Detected {} landmarks.", shape.num_parts());

    // Step 2: Convert to eos landmarks
    eos::core::LandmarkCollection<Eigen::Vector2f> landmarks;
    std::vector<Eigen::Vector3f> depth_points;
    for (int i = 0; i < shape.num_parts(); ++i) {
        float x = static_cast<float>(shape.part(i).x());
        float y = static_cast<float>(shape.part(i).y());
        float depth = depthFrame.get_distance((int)x, (int)y);

        // Backproject using intrinsics (from pixel to 3D)
        float point3D[3];
        float pixel[2] = { x, y };
        cv::Point2f pixel_uv; // (u, v)

        float fx = cameraIntrinsics.at<float>(0, 0);
        float fy = cameraIntrinsics.at<float>(1, 1);
        float cx = cameraIntrinsics.at<float>(0, 2);
        float cy = cameraIntrinsics.at<float>(1, 2);

        point3D[0] = (pixel_uv.x - cx) * depth / fx;
        point3D[1] = (pixel_uv.y - cy) * depth / fy;
        point3D[2] = depth;

        depth_points.emplace_back(point3D[0], point3D[1], point3D[2]);

        eos::core::Landmark<Eigen::Vector2f> lm;
        lm.name = std::to_string(i + 1);
        lm.coordinates = Eigen::Vector2f(shape.part(i).x(), shape.part(i).y());
        landmarks.push_back(lm);

    }
    spdlog::info("[fitAndRender] Converted to eos::LandmarkCollection.");

    // Step 3: Fit shape and pose
    eos::core::Mesh mesh;
    eos::fitting::RenderingParameters rendering_params;
    std::tie(mesh, rendering_params) = eos::fitting::fit_shape_and_pose(
        morphable_model_, landmarks, landmark_mapper_,
        colorImage.cols, colorImage.rows, edge_topology_,
        ibug_contour_, model_contour_,
        5, eos::cpp17::nullopt, 30.0f);
    spdlog::info("[fitAndRender] Fitting done. Mesh has {} vertices and {} faces.",
        mesh.vertices.size(), mesh.tvi.size());
    
    // Step 4: Texture extraction
    const auto texturemap = eos::render::extract_texture(
        mesh, rendering_params.get_modelview(), rendering_params.get_projection(),
        eos::render::ProjectionType::Orthographic, eos::core::from_mat_with_alpha(colorImage));

    // Step 5: Upload vertex data with position + normal + texcoord
    // Compute vertex normals
    // Upload vertex data
    std::vector<float> vertexData; // interleaved: pos (3) + normal (3) + texcoord (2)
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        const auto& uv = mesh.texcoords.empty() ? Eigen::Vector2f(0, 0) : mesh.texcoords[i];
        Eigen::Vector3f n(0, 0, 1); // use dummy normal or compute manually

        vertexData.insert(vertexData.end(), {v[0], v[1], v[2], n[0], n[1], n[2], uv[0], uv[1]});
    }

    std::vector<unsigned short> indices;
    for (const auto& tri : mesh.tvi) {
        indices.push_back(static_cast<unsigned short>(tri[0]));
        indices.push_back(static_cast<unsigned short>(tri[1]));
        indices.push_back(static_cast<unsigned short>(tri[2]));
    }

    // Generate buffers
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    GLuint ebo = 0; glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), indices.data(), GL_STATIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    // TexCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
    spdlog::info("[fitAndRender] Uploaded {} vertices to OpenGL.", faceVertexCount);

    // Step 6: Upload texture
    if (texture == 0) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    auto texmat = eos::core::to_mat(texturemap);
    if (texmat.empty()) {
        spdlog::error("[fitAndRender] Texture extraction failed.");
        return;
    }
    spdlog::info("[fitAndRender] Texture map size: {}x{}", texmat.cols, texmat.rows);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texmat.cols, texmat.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmat.data);

    // Step 7: Compute Model Matrix (camera-aligned placeholder)
    model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, glm::vec3(0.0f, 0.0f, -3.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(5.0f));
}

}