// makeup_transfer.hpp
#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <GLES2/gl2.h>

struct Intrinsics {
    float cx, cy, fx, fy;
    int width, height;
};

namespace makeup {

// Apply pre-extracted UV makeup to a target face using 2D landmarks
cv::Mat apply_makeup_from_uv(
    const std::vector<cv::Point2f>& uv_points,
    const std::vector<cv::Point2f>& image_points,
    const cv::Mat& makeup_texture,
    const cv::Mat& alpha_texture,
    const cv::Mat& target);

// Project UV makeup texture into a new view based on 3D landmarks and camera pose
cv::Mat warp_uv_makeup_to_target_view(
    const std::vector<cv::Point3f>& uv_points_3d,
    const std::vector<cv::Point2f>& uv_coords_2d,
    const cv::Mat& makeup_texture,
    const cv::Mat& alpha_texture,
    const cv::Mat& target_image,
    const cv::Mat& R, const cv::Mat& t,
    const Intrinsics& intr);

// Initialize OpenGL ES context (EGL)
bool initGLESContext();

// Render UV makeup on a 3D face mesh using OpenGL ES
void renderMakeupOverlay(
    GLuint vbo,                   // Vertex buffer object for 3D face mesh positions
    GLuint uvbo,                  // Vertex buffer object for UV coordinates
    GLuint makeupTex,            // Makeup texture (GL_TEXTURE_2D)
    GLuint alphaTex,             // Alpha texture (GL_TEXTURE_2D)
    const float* mvpMatrix,      // Model-view-projection matrix (4x4)
    int viewportWidth,           // Output viewport width
    int viewportHeight           // Output viewport height
);

} // namespace makeup
