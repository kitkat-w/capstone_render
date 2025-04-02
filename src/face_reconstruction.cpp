#include "face_reconstruction.hpp"

#include "eos/core/Image.hpp"
#include "eos/core/image/opencv_interop.hpp"
#include "eos/core/Landmark.hpp"
#include "eos/core/LandmarkMapper.hpp"
#include "eos/core/read_pts_landmarks.hpp"
#include "eos/core/write_obj.hpp"
#include "eos/fitting/fitting.hpp"
#include "eos/morphablemodel/Blendshape.hpp"
#include "eos/morphablemodel/MorphableModel.hpp"
#include "eos/render/opencv/draw_utils.hpp"
#include "eos/render/texture_extraction.hpp"
#include "eos/cpp17/optional.hpp"

#include <glad/glad.h>
#include <spdlog/spdlog.h>

namespace UsArMirror {

using namespace eos;

FaceReconstruction::FaceReconstruction(const std::string& model_path) {
    loadEOSModels(model_path);
}

void FaceReconstruction::loadEOSModels(const std::string& path) {
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

void FaceReconstruction::reconstructFromLandmarks(const dlib::full_object_detection& shape, const cv::Mat& texture_img) {
    // Convert landmarks
    core::LandmarkCollection<Eigen::Vector2f> landmarks;
    for (int i = 0; i < shape.num_parts(); ++i) {
        core::Landmark<Eigen::Vector2f> lm;
        lm.name = std::to_string(i + 1);
        lm.coordinates = Eigen::Vector2f(shape.part(i).x(), shape.part(i).y());
        landmarks.emplace_back(std::move(lm));
    }

    // Fit
    eos::core::Mesh mesh;
    eos::fitting::RenderingParameters render_params;
    std::tie(mesh, render_params) = eos::fitting::fit_shape_and_pose(
        morphable_model_, landmarks, landmark_mapper_,
        texture_img.cols, texture_img.rows,
        edge_topology_, ibug_contour_, model_contour_,
        5, cpp17::nullopt, 30.0f
    );

    // Store MVP
    modelview_ = render_params.get_modelview();
    projection_ = render_params.get_projection();

    // Extract texture
    const auto texmap = eos::render::extract_texture(
        mesh, modelview_, projection_,
        eos::render::ProjectionType::Orthographic,
        eos::core::from_mat_with_alpha(texture_img));

    mesh.texture = texmap;

    uploadMeshToGPU(mesh);
    uploadTextureToGPU(texmap);
}

void FaceReconstruction::uploadMeshToGPU(const core::Mesh& mesh) {
    // Generate buffer data
    std::vector<float> verts, uvs;
    std::vector<unsigned int> indices;

    for (const auto& v : mesh.vertices) {
        verts.insert(verts.end(), {v[0], v[1], v[2]});
    }
    for (const auto& t : mesh.texcoords) {
        uvs.insert(uvs.end(), {t[0], t[1]});
    }
    for (const auto& tvi : mesh.tvi) {
        indices.push_back(tvi[0]);
        indices.push_back(tvi[1]);
        indices.push_back(tvi[2]);
    }

    index_count_ = indices.size();

    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (tbo_) glDeleteBuffers(1, &tbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &tbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, tbo_);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void FaceReconstruction::uploadTextureToGPU(const eos::core::Image4u& img) {
    if (texture_id_) glDeleteTextures(1, &texture_id_);
    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data()->data());
}

void FaceReconstruction::render() {
    if (index_count_ == 0 || !vao_ || !texture_id_) return;

    glUseProgram(shader_program_);
    glUniformMatrix4fv(glGetUniformLocation(shader_program_, "MVP"), 1, GL_FALSE, (projection_ * modelview_).data());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glUniform1i(glGetUniformLocation(shader_program_, "faceTexture"), 0);

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

}
