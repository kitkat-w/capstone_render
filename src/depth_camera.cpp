#include "depth_camera.hpp"

#include <iostream>
#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>

namespace UsArMirror {

DepthCameraInput::DepthCameraInput(const std::shared_ptr<State>& state, int idx)
    : state(state), running(true), textureId(-1), depth_frame(rs2::frame()) {
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

        faceNet = cv::dnn::readNetFromCaffe(
            "deploy.prototxt",
            "res10_300x300_ssd_iter_140000.caffemodel");

        facemark = cv::face::FacemarkLBF::create();
        facemark->loadModel("lbfmodel.yaml");
        spdlog::info("Loaded OpenCV FacemarkLBF model");

    } catch (const rs2::error& e) {
        throw std::runtime_error(std::string("RealSense error: ") + e.what());
    }

    createGlTexture();
    captureThread = std::thread(&DepthCameraInput::captureLoop, this);
    detectionThread = std::thread(&DepthCameraInput::detectionLoop, this);
}

DepthCameraInput::~DepthCameraInput() {
    running = false;

    if (captureThread.joinable()) captureThread.join();
    if (detectionThread.joinable()) detectionThread.join();

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
                std::lock_guard lock(frameMutex);
                frame = raw.clone();
            }

            if (depth) {
                std::lock_guard lock(frameMutex);
                depth_frame = depth;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void DepthCameraInput::detectionLoop() {
    while (running) {
        cv::Mat currentFrame;
        {
            std::lock_guard lock(frameMutex);
            if (frame.empty()) continue;
            currentFrame = frame.clone();
        }

        cv::Mat blob = cv::dnn::blobFromImage(currentFrame, 1.0, cv::Size(300, 300), cv::Scalar(104.0, 177.0, 123.0), false, false);
        faceNet.setInput(blob);
        cv::Mat detections = faceNet.forward();

        std::vector<cv::Rect> faces;
        cv::Mat detectionMat(detections.size[2], detections.size[3], CV_32F, detections.ptr<float>());

        for (int i = 0; i < detectionMat.rows; ++i) {
            float confidence = detectionMat.at<float>(i, 2);
            if (confidence > 0.9f) {
                int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * currentFrame.cols);
                int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * currentFrame.rows);
                int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * currentFrame.cols);
                int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * currentFrame.rows);
                faces.emplace_back(cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2)));
            }
        }

        std::vector<std::vector<cv::Point2f>> landmarks;
        if (facemark->fit(currentFrame, faces, landmarks)) {
            std::lock_guard lock(faceMutex);
            faceBoxes = faces;
            landmarkPoints = landmarks;
            for (const auto& pt : landmarkPoints[0]) {
                std::cout << "Point: " << pt << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

bool DepthCameraInput::getFrame(cv::Mat& outputFrame) {
    std::lock_guard lock(frameMutex);
    if (!frame.empty()) {
        outputFrame = frame.clone();

        std::lock_guard faceLock(faceMutex);
        for (size_t i = 0; i < faceBoxes.size(); ++i) {
            // cv::rectangle(outputFrame, faceBoxes[i], cv::Scalar(0, 255, 0), 2);
            std::cout << landmarkPoints[i].size() << " landmarks for face " << i << std::endl;
            for (const auto& pt : landmarkPoints[i]) {
                cv::circle(outputFrame, pt, 2, cv::Scalar(255, 0, 0), -1);
                if (pt.x >= 0 && pt.x < outputFrame.cols && pt.y >= 0 && pt.y < outputFrame.rows) {
                    std::cout << "Point: " << pt << std::endl;
                } else {
                    std::cout << "Invalid point: " << pt << std::endl;
                }
            }
        }

        return true;
    }
    return false;
}

rs2::depth_frame DepthCameraInput::getDepth() {
    std::lock_guard lock(frameMutex);
    return depth_frame;
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

        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(1.0f, 1.0f);
        glEnd();
    }
}

} // namespace UsArMirror
