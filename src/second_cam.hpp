#pragma once

#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <optional>
#include <memory>
#include <glad/glad.h>

#include "common.hpp"

namespace UsArMirror {

class CameraInput {
public:
    CameraInput(const std::shared_ptr<State>& state, int idx);
    CameraInput(const std::shared_ptr<State>& state, int idx, int rotateCode);
    ~CameraInput();

    bool getFrame(cv::Mat& outputFrame);
    void render(); // Renders camera feed (defaults to right-half of screen)

    int width, height;

    struct Intrinsics {
        float fx = 643.18665255f;
        float fy = 639.97735531f;
        float cx = 297.94373267f;
        float cy = 210.9169765f;
        int width = 640;
        int height = 480;
        std::array<float, 5> dist = {-0.43676459f,0.3501338f, 0.00247679f, 0.00335949f, -0.31811064f};
        cv::Mat getK() const {
            return (cv::Mat_<float>(3, 3) <<
                fx, 0, cx,
                0, fy, cy,
                0, 0, 1);
        }
        
        cv::Mat getDist() const {
            return cv::Mat(1, 5, CV_32F, (void*)dist.data()).clone();
        }
    };

private:
    void captureLoop();
    void createGlTexture();

    std::shared_ptr<State> state;
    bool running;
    cv::VideoCapture cap;

    cv::Mat frame;
    std::mutex frameMutex;

    GLuint textureId;
    std::thread captureThread;

    std::optional<int> rotateCode;
};

} // namespace UsArMirror
