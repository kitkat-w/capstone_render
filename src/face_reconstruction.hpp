#pragma once

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

#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/opencv.h>

#include <librealsense2/rs.hpp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <glm/glm.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace UsArMirror {

    class FaceReconstruction {
    public:
        FaceReconstruction(const std::string& model_path);
    
        void fitAndRender(
            const cv::Mat& colorImage,
            const rs2::depth_frame& depthFrame,
            const cv::Mat& cameraIntrinsics,
            const cv::Mat& extrinsicsDepthToColor,
            std::pair<GLuint, std::map<int, GLuint>>& vaoAndVbos,
            GLuint& vao,
            GLuint& vbo,
            GLuint& ebo,
            GLuint& texture,
            glm::mat4& model_matrix,
            int width,
            int height,
            int& faceVertexCount);
    
        std::pair<GLuint, std::map<int, GLuint>> bindEosMesh(
            const std::vector<float>& vertexData,
            const std::vector<unsigned short>& indices);
    
        void drawEosMesh(
            const std::pair<GLuint, std::map<int, GLuint>>& vaoAndVbos,
            int indexCount);
    
    private:
        void loadEOSModels(const std::string& path);
        void generateMeshFromCoefficients(
            const std::vector<float>& coefficients,
            const std::vector<unsigned short>& indices,
            const std::vector<float>& vertexData);
    
        eos::morphablemodel::MorphableModel morphable_model_;
        eos::core::LandmarkMapper landmark_mapper_;
        eos::morphablemodel::EdgeTopology edge_topology_;
        eos::fitting::ContourLandmarks ibug_contour_;
        eos::fitting::ModelContour model_contour_;
    
        dlib::frontal_face_detector detector;
        dlib::shape_predictor predictor;
    };
    
    } // namespace UsArMirror
    