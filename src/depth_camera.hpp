#pragma once

#include <opencv2/opencv.hpp>
#include <glad/glad.h>
#include <memory>
#include <thread>
#include <mutex>
#include <optional>
#include <librealsense2/rs.hpp>

#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>

#include "common.hpp"

namespace UsArMirror {

struct DepthCameraInputImpl {
  rs2::pipeline pipe;
  rs2::colorizer color_map;
  rs2::frameset frames;
  rs2::frame color_frame;
  rs2::frame depth_frame;
};

struct Intrinsics {
  float fx = 615.0f;  // focal length in pixels (x)
  float fy = 615.0f;  // focal length in pixels (y)
  float cx = 320.0f;  // optical center x (half of width)
  float cy = 240.0f;  // optical center y (half of height)
  int width = 640;
  int height = 480;
};

class DepthCameraInput {
  public:
    explicit DepthCameraInput(const std::shared_ptr<State>& state, int idx);
    ~DepthCameraInput();

    bool getFrame(cv::Mat &outputFrame);
    void render();

    int width, height;

    cv::Mat getLastColorFrame() const;

    dlib::full_object_detection landmarks;
    bool hasLandmarks = false;

  private:
    std::shared_ptr<State> state;
    bool running;
    std::mutex frameMutex;
    std::thread captureThread;
    cv::Mat frame;
    GLuint textureId;
    dlib::frontal_face_detector detector;
    dlib::shape_predictor predictor;

    // struct DepthCameraInputImpl;
    std::unique_ptr<DepthCameraInputImpl> impl;
    cv::Mat detectLandmarks(
      cv::Mat processed);

    void createGlTexture();
    void captureLoop();
    rs2::frame getDepth();

};
} // namespace UsArMirror