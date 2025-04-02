#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
// #include <open3d/Open3D.h>
#include <iostream>
#include <chrono>

using namespace std;

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
    // Initialize RealSense pipeline
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
            std::vector<cv::Point2f> landmarks_2d;
            std::vector<cv::Point3f> landmarks_3d;

            for (int i = 0; i < shape.num_parts(); ++i) {
                int x = shape.part(i).x();
                int y = shape.part(i).y();
                landmarks_2d.emplace_back(x, y);
                uint16_t depth_val = depth.at<uint16_t>(y, x);

                Intrinsics intr = {319.5, 239.5, 600.0, 600.0, 640, 480}; // You can replace with actual intrinsics
                cv::Point3f point_3d = depth_to_3d(x, y, depth_val, intr);
                landmarks_3d.push_back(point_3d);

                cv::circle(color, cv::Point(x, y), 2, cv::Scalar(0, 255, 0), -1);
            }

            cv::imshow("RGB with Landmarks", color);
            if (cv::waitKey(1) == 27) break;
        }
    }

    return 0;
}
