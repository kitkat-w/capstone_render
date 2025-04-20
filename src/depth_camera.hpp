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

class DepthCameraInput {
  public:
    explicit DepthCameraInput(const std::shared_ptr<State>& state, int idx);
    ~DepthCameraInput();

    bool getFrame(cv::Mat &outputFrame);
    void render();

    int width, height;

    cv::Mat getLastColorFrame() const;
    rs2::depth_frame getDepth();

    dlib::full_object_detection landmarks;
    bool hasLandmarks = false;

    struct Intrinsics {
      float fx = 302.02243162f;
      float fy = 301.80520504f;
      float cx = 324.73866457f;
      float cy = 216.85437825f;
      int width = 640;
      int height = 480;
    };

    Intrinsics intrinsics;

  private:
    std::shared_ptr<State> state;
    bool running;
    std::mutex frameMutex;
    std::thread captureThread;
    cv::Mat frame;
    rs2::depth_frame depth_frame;
    GLuint textureId;
    dlib::frontal_face_detector detector;
    dlib::shape_predictor predictor;

    // struct DepthCameraInputImpl;
    std::unique_ptr<DepthCameraInputImpl> impl;
    cv::Mat detectLandmarks(
      cv::Mat processed);

    void createGlTexture();
    void captureLoop();

    int frameCount = 0; 
    std::thread landmarkThread;
    std::atomic<bool> detectRunning;
    std::mutex landmarkMutex;

    std::vector<dlib::point> landmark2D;
    std::vector<cv::Point3f> landmark3D;

    std::vector<dlib::point> getLandmarks2D();
    std::vector<cv::Point3f> getLandmarks3D();
    void landmarkLoop();


};
} // namespace UsArMirror