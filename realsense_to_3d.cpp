// realsense_eos_fit.cpp

#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
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

#include "Eigen/Core"

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgcodecs/imgcodecs.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>

using namespace std;
using namespace eos;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using eos::core::Landmark;
using eos::core::LandmarkCollection;
using cv::Mat;
using std::cout;
using std::endl;
using std::string;
using std::vector;

struct Intrinsics {
    float cx, cy, fx, fy;
    int width, height;
};

cv::Point3f depth_to_3d(int x, int y, uint16_t depth_val, Intrinsics intr) {
    float z = depth_val * 0.001f;
    if (z <= 0) return cv::Point3f(NAN, NAN, NAN);
    float X = (x - intr.cx) * z / intr.fx;
    float Y = (y - intr.cy) * z / intr.fy;
    return cv::Point3f(X, Y, z);
}

int main() {
    rs2::pipeline pipe;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);
    cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);
    pipe.start(cfg);

    dlib::frontal_face_detector detector = dlib::get_frontal_face_detector();
    dlib::shape_predictor predictor;
    try {
        dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> predictor;
    } catch (dlib::serialization_error& e) {
        std::cerr << "Could not load landmark model: " << e.what() << std::endl;
        return 1;
    }

    morphablemodel::MorphableModel morphable_model = morphablemodel::load_model("share/sfm_shape_3448.bin");
    vector<morphablemodel::Blendshape> blendshapes = morphablemodel::load_blendshapes("share/expression_blendshapes_3448.bin");
    core::LandmarkMapper landmark_mapper("share/ibug_to_sfm.txt");
    morphablemodel::EdgeTopology edge_topology = morphablemodel::load_edge_topology("share/sfm_3448_edge_topology.json");
    fitting::ContourLandmarks ibug_contour = fitting::ContourLandmarks::load("share/ibug_to_sfm.txt");
    fitting::ModelContour model_contour = fitting::ModelContour::load("share/sfm_model_contours.json");

    morphablemodel::MorphableModel morphable_model_with_expressions(
        morphable_model.get_shape_model(), blendshapes, morphable_model.get_color_model(), cpp17::nullopt,
        morphable_model.get_texture_coordinates());

    Intrinsics intr = {319.5f, 239.5f, 600.0f, 600.0f, 640, 480};

    while (true) {
        rs2::frameset frames = pipe.wait_for_frames();
        rs2::frame color_frame = frames.get_color_frame();
        rs2::frame depth_frame = frames.get_depth_frame();

        cv::Mat color(cv::Size(640, 480), CV_8UC3, (void*)color_frame.get_data(), cv::Mat::AUTO_STEP);
        cv::Mat depth(cv::Size(640, 480), CV_16UC1, (void*)depth_frame.get_data(), cv::Mat::AUTO_STEP);

        dlib::cv_image<dlib::bgr_pixel> dlib_img(color);
        std::vector<dlib::rectangle> faces = detector(dlib_img);

        if (!faces.empty()) {
            auto shape = predictor(dlib_img, faces[0]);
            core::LandmarkCollection<Eigen::Vector2f> landmarks;

            for (int i = 0; i < shape.num_parts(); ++i) {
                int x = shape.part(i).x();
                int y = shape.part(i).y();
                eos::core::Landmark<Eigen::Vector2f> lm;
                lm.name = std::to_string(i + 1);
                lm.coordinates = Eigen::Vector2f(x, y);
                landmarks.emplace_back(std::move(lm));

                cv::circle(color, cv::Point(x, y), 2, cv::Scalar(0, 255, 0), -1);
            }

            core::Mesh mesh;
            fitting::RenderingParameters rendering_params;

            std::tie(mesh, rendering_params) = fitting::fit_shape_and_pose(
                morphable_model_with_expressions, landmarks, landmark_mapper,
                color.cols, color.rows, edge_topology, ibug_contour, model_contour,
                5, cpp17::nullopt, 30.0f);

            const core::Image4u texturemap = render::extract_texture(
                mesh,
                rendering_params.get_modelview(),
                rendering_params.get_projection(),
                render::ProjectionType::Orthographic,
                core::from_mat_with_alpha(color)
            );

            // Update mesh.texcoords and mesh.tci
            mesh.texcoords = mesh.texcoords.empty() ? std::vector<Eigen::Vector2f>(mesh.vertices.size(), {0.5f, 0.5f}) : mesh.texcoords;
            if (mesh.tci.empty() && mesh.tvi.size() == mesh.texcoords.size() / 3) {
                mesh.tci = mesh.tvi; // fallback to copy tvi
            }

            fs::path output_basename = "output/fitted_face";
            fs::create_directories(output_basename.parent_path());

            fs::path obj_file = output_basename;
            obj_file.replace_extension(".obj");
            core::write_textured_obj(mesh, obj_file.string());

            fs::path texture_file = output_basename;
            texture_file.replace_extension(".texture.png");
            cv::imwrite(texture_file.string(), core::to_mat(texturemap));

            render::draw_wireframe(color, mesh, rendering_params.get_modelview(), rendering_params.get_projection(),
                                   fitting::get_opencv_viewport(color.cols, color.rows));

            cv::imshow("Fitted Mesh", color);
            if (cv::waitKey(1) == 27) break;

            return 0;
        }
    }

    return 0;
}
