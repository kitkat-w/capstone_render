#include "secondary_camera.hpp"

SecondaryCameraInput::SecondaryCameraInput(int camIndex) : running(true) {
    cap.open(camIndex);
    if (!cap.isOpened()) {
        throw std::runtime_error("Failed to open secondary camera");
    }

    captureThread = std::thread(&SecondaryCameraInput::captureLoop, this);
}

SecondaryCameraInput::~SecondaryCameraInput() {
    running = false;
    if (captureThread.joinable()) captureThread.join();
    cap.release();
}

void SecondaryCameraInput::captureLoop() {
    while (running) {
        cv::Mat newFrame;
        cap >> newFrame;
        if (!newFrame.empty()) {
            std::lock_guard<std::mutex> lock(frameMutex);
            frame = newFrame.clone();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

bool SecondaryCameraInput::getFrame(cv::Mat& frameOut) {
    std::lock_guard<std::mutex> lock(frameMutex);
    if (!frame.empty()) {
        frameOut = frame.clone();
        return true;
    }
    return false;
}
