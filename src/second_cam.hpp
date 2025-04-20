#pragma once

#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <atomic>

class SecondaryCameraInput {
public:
    SecondaryCameraInput(int camIndex = 1);
    ~SecondaryCameraInput();

    bool getFrame(cv::Mat& frameOut);

private:
    void captureLoop();

    std::thread captureThread;
    std::mutex frameMutex;
    std::atomic<bool> running;
    cv::Mat frame;
    cv::VideoCapture cap;
};
