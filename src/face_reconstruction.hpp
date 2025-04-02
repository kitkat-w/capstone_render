#pragma once

#include <dlib/image_processing.h>
#include <opencv2/opencv.hpp>
#include <string>

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

namespace UsArMirror {

class FaceReconstruction {
public:
    explicit FaceReconstruction(const std::string& model_path);

    // Main API: call this to fit and upload texture+mesh
    void reconstructFromLandmarks(const dlib::full_object_detection& landmarks, const cv::Mat& texture);

    // Call this from render loop
    void render();

private:
    void loadEOSModels(const std::string& model_path);
    void uploadMeshToGPU(const eos::core::Mesh& mesh);
    void uploadTextureToGPU(const eos::core::Image4u& texture);

    // EOS
    eos::morphablemodel::MorphableModel morphable_model_;
    std::vector<eos::morphablemodel::Blendshape> blendshapes_;
    eos::core::LandmarkMapper landmark_mapper_;
    eos::morphablemodel::EdgeTopology edge_topology_;
    eos::fitting::ContourLandmarks ibug_contour_;
    eos::fitting::ModelContour model_contour_;

    // OpenGL
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint tbo_ = 0;
    GLuint ebo_ = 0;
    GLuint texture_id_ = 0;
    GLuint shader_program_ = 0;
    size_t index_count_ = 0;

    // MVP matrices
    Eigen::Matrix4f modelview_;
    Eigen::Matrix4f projection_;

};

}
