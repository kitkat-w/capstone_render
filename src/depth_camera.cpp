#include "depth_camera.hpp"

#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>

namespace UsArMirror {

DepthCameraInput::DepthCameraInput(const std::shared_ptr<State>& state, int idx)
    : state(state), running(true), detectRunning(true), textureId(-1), depth_frame(rs2::frame()) {
    try {
        impl = std::make_unique<DepthCameraInputImpl>();
        rs2::config cfg;
        cfg.enable_stream(RS2_STREAM_COLOR, state->viewportWidth, state->viewportHeight, RS2_FORMAT_BGR8, 30);
        cfg.enable_stream(RS2_STREAM_DEPTH, state->viewportWidth, state->viewportHeight, RS2_FORMAT_Z16, 30);
        spdlog::info("Trying to start RealSense pipeline...");
        impl->pipe.start(cfg);

        width = state->viewportWidth;
        height = state->viewportHeight;

        spdlog::info("RealSense camera started: width={}, height={}", width, height);

        detector = dlib::get_frontal_face_detector();
        try {
            dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> predictor;
        } catch (dlib::serialization_error& e) {
            throw std::runtime_error(std::string("Could not load landmark model: ") + e.what());
        }

        spdlog::info("Deserialized dlib predictor");
    } catch (const rs2::error& e) {
        throw std::runtime_error(std::string("RealSense error: ") + e.what());
    }

    createGlTexture();
    captureThread = std::thread(&DepthCameraInput::captureLoop, this);
    landmarkThread = std::thread(&DepthCameraInput::landmarkLoop, this);
}

DepthCameraInput::~DepthCameraInput() {
    running = false;
    detectRunning = false;

    if (captureThread.joinable()) {
        captureThread.join();
    }
    if (landmarkThread.joinable()) {
        landmarkThread.join();
    }

    if (impl) {
        impl->pipe.stop();
    }
}

void DepthCameraInput::createGlTexture() {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
}

void DepthCameraInput::captureLoop() {
    while (running) {
        if (!impl) continue;

        if (impl->pipe.poll_for_frames(&impl->frames)) {
            rs2::video_frame color = impl->frames.get_color_frame();
            rs2::depth_frame depth = impl->frames.get_depth_frame();

            if (color) {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(color.get_data());
                cv::Mat raw(color.get_height(), color.get_width(), CV_8UC3, (void*)data, cv::Mat::AUTO_STEP);
                cv::Mat processed = raw.clone();

                std::lock_guard lock(frameMutex);
                frame = processed;
            }
            if (depth) {
                std::lock_guard lock(frameMutex);
                depth_frame = depth;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void DepthCameraInput::landmarkLoop() {
    while (detectRunning) {
        cv::Mat currentFrame, depthMat;

        {
            std::lock_guard lock(frameMutex);
            if (frame.empty() || !depth_frame) continue;

            currentFrame = frame.clone();
            depthMat = cv::Mat(depth_frame.get_height(), depth_frame.get_width(), CV_16UC1,
                               (void*)depth_frame.get_data(), cv::Mat::AUTO_STEP).clone();
        }

        auto start = std::chrono::high_resolution_clock::now();
        dlib::cv_image<dlib::bgr_pixel> dlib_img(currentFrame);
        std::vector<dlib::rectangle> faces = detector(dlib_img);
        if (faces.empty()) continue;

        dlib::full_object_detection shape = predictor(dlib_img, faces[0]);

        auto end = std::chrono::high_resolution_clock::now(); 
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        spdlog::info("Detect face took {} µs", duration);

        std::vector<dlib::point> points2D;
        std::vector<cv::Point3f> points3D;

        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < shape.num_parts(); ++i) {
            int x = shape.part(i).x();
            int y = shape.part(i).y();

            if (x < 0 || x >= depthMat.cols || y < 0 || y >= depthMat.rows) continue;

            uint16_t d = depthMat.at<uint16_t>(y, x);
            if (d == 0) continue;

            float depth_m = d * 0.001f;

            float px = (x - intrinsics.cx) / intrinsics.fx;
            float py = (y - intrinsics.cy) / intrinsics.fy;
            cv::Point3f pt3d(px * depth_m, py * depth_m, depth_m);

            points2D.push_back(shape.part(i));
            points3D.push_back(pt3d);
        }

        end = std::chrono::high_resolution_clock::now(); 
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        spdlog::info("Convert to 3d took {} µs", duration);

        {
            std::lock_guard lock(landmarkMutex);
            landmark2D = std::move(points2D);
            landmark3D = std::move(points3D);
            hasLandmarks = true;
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

bool DepthCameraInput::getFrame(cv::Mat& outputFrame) {
    std::lock_guard lock(frameMutex);
    if (!frame.empty()) {
        outputFrame = frame.clone();

        if (hasLandmarks) {
            auto start = std::chrono::high_resolution_clock::now();  // ⏱️ start timer

            std::lock_guard lmLock(landmarkMutex);
            for (const auto& pt : landmark2D) {
                cv::circle(outputFrame, cv::Point(pt.x(), pt.y()), 2, cv::Scalar(0, 255, 0), -1);
            }

            auto end = std::chrono::high_resolution_clock::now();  // ⏱️ stop timer
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            // spdlog::info("Landmark overlay took {} µs", duration);
        }

        return true;
    }

    spdlog::info("Frame is empty");
    return false;
}


rs2::depth_frame DepthCameraInput::getDepth() {
    std::lock_guard lock(frameMutex);
    if (depth_frame && depth_frame.is<rs2::depth_frame>()) {
        return depth_frame;
    } else {
        spdlog::warn("Depth frame is empty or invalid.");
        return rs2::frame();  // return an explicitly empty frame
    }
}

std::vector<dlib::point> DepthCameraInput::getLandmarks2D() {
    std::lock_guard lock(landmarkMutex);
    return landmark2D;
}

std::vector<cv::Point3f> DepthCameraInput::getLandmarks3D() {
    std::lock_guard lock(landmarkMutex);
    return landmark3D;
}

cv::Mat DepthCameraInput::getLastColorFrame() const {
    return frame.empty() ? cv::Mat() : frame.clone();
}

void DepthCameraInput::render() {
    cv::Mat frame;
    if (getFrame(frame)) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
        {
            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(1.0f, -1.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(1.0f, 1.0f);
            glEnd();
        }
    }
}

} // namespace UsArMirror
