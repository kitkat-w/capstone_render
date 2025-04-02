#include "depth_camera.hpp"

#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>

namespace UsArMirror {

DepthCameraInput::DepthCameraInput(const std::shared_ptr<State>& state, int idx)
    : state(state), running(true), textureId(-1) {
    // RealSense pipeline setup
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

    // Setup OpenGL texture
    createGlTexture();
    captureThread = std::thread(&DepthCameraInput::captureLoop, this);
}

DepthCameraInput::~DepthCameraInput() {
    running = false;
    if (captureThread.joinable()) {
        captureThread.join();
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
            if (color) {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(color.get_data());
                cv::Mat raw(color.get_height(), color.get_width(), CV_8UC3, (void*)data, cv::Mat::AUTO_STEP);
                cv::Mat processed = raw.clone();

                std::lock_guard lock(frameMutex);
                frame = processed;
            }
        }
    }
}


// bool DepthCameraInput::captureDepthFrame(cv::Mat &depthOutput) {
//     if (!impl) return false;
//     rs2::frameset frames = impl->pipe.wait_for_frames();
//     rs2::depth_frame depth = frames.get_depth_frame();
//     if (!depth) return false;

//     cv::Mat depthMat(cv::Size(depth.get_width(), depth.get_height()), CV_16UC1, (void*)depth.get_data(), cv::Mat::AUTO_STEP);
//     depthOutput = depthMat.clone();
//     return true;
// }

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

rs2::frame DepthCameraInput::getDepth() {
    std::lock_guard lock(frameMutex);
    auto output = impl->depth_frame;
    return output;
}

bool DepthCameraInput::getFrame(cv::Mat &outputFrame) {
    std::lock_guard lock(frameMutex);
    if (!frame.empty()) {
        auto temp = frame.clone();
        outputFrame = detectLandmarks(temp);
        cv::imwrite("frame_001.png", outputFrame);
        return true;
    }
    spdlog::info("Frame is empty");
    return false;
}

cv::Mat DepthCameraInput::detectLandmarks(
   cv::Mat processed)
{
    // Run dlib face detection
    dlib::cv_image<dlib::rgb_pixel> dlib_img(processed);
    std::vector<dlib::rectangle> faces = detector(dlib_img);
    spdlog::info("Dlib detected {} face(s)", faces.size());
    cv::imwrite("frame_002.png", processed);

    hasLandmarks = false;
    if (!faces.empty()) {
        landmarks = predictor(dlib_img, faces[0]);
        hasLandmarks = true;

        // Draw landmarks
        for (unsigned int i = 0; i < landmarks.num_parts(); ++i) {
            const auto& pt = landmarks.part(i);
            cv::circle(processed, cv::Point(pt.x(), pt.y()), 2, cv::Scalar(0, 255, 0), -1);
        }
    }
    return processed;
}


} // namespace UsArMirror
