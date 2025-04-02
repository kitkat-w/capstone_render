// makeup_transfer.cpp
#include "makeup_transfer.hpp"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <stdexcept>
#include <iostream>

namespace makeup {

cv::Mat apply_makeup_from_uv(
    const std::vector<cv::Point2f>& uv_points,
    const std::vector<cv::Point2f>& image_points,
    const cv::Mat& makeup_texture,
    const cv::Mat& alpha_texture,
    const cv::Mat& target) {

    if (uv_points.size() != image_points.size() || uv_points.size() < 3) {
        throw std::runtime_error("Insufficient or mismatched landmark points");
    }

    cv::Mat affine = cv::estimateAffine2D(uv_points, image_points);

    cv::Mat warped_makeup, warped_alpha;
    cv::warpAffine(makeup_texture, warped_makeup, affine, target.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);
    cv::warpAffine(alpha_texture, warped_alpha, affine, target.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);

    cv::Mat alpha_f;
    warped_alpha.convertTo(alpha_f, CV_32FC1, 1.0 / 255.0);
    cv::Mat makeup_f, target_f;
    warped_makeup.convertTo(makeup_f, CV_32FC3, 1.0 / 255.0);
    target.convertTo(target_f, CV_32FC3, 1.0 / 255.0);

    cv::Mat result_f(target.size(), CV_32FC3);
    for (int y = 0; y < target.rows; ++y) {
        for (int x = 0; x < target.cols; ++x) {
            float a = alpha_f.at<float>(y, x);
            cv::Vec3f src = makeup_f.at<cv::Vec3f>(y, x);
            cv::Vec3f dst = target_f.at<cv::Vec3f>(y, x);
            result_f.at<cv::Vec3f>(y, x) = a * src + (1.0f - a) * dst;
        }
    }

    cv::Mat result;
    result_f.convertTo(result, CV_8UC3, 255.0);
    return result;
}

cv::Mat warp_uv_makeup_to_target_view(
    const std::vector<cv::Point3f>& uv_points_3d,
    const std::vector<cv::Point2f>& uv_coords_2d,
    const cv::Mat& makeup_texture,
    const cv::Mat& alpha_texture,
    const cv::Mat& target_image,
    const cv::Mat& R, const cv::Mat& t,
    const Intrinsics& intr) {

    if (uv_points_3d.size() != uv_coords_2d.size()) {
        throw std::runtime_error("Mismatch between 3D UV points and 2D UV coordinates");
    }

    std::vector<cv::Point2f> projected_2d;
    std::vector<cv::Point2f> sampled_makeup_coords;

    for (size_t i = 0; i < uv_points_3d.size(); ++i) {
        const auto& pt = uv_points_3d[i];
        cv::Mat p = (cv::Mat_<double>(3,1) << pt.x, pt.y, pt.z);
        cv::Mat pt_trans = R * p + t;

        double X = pt_trans.at<double>(0);
        double Y = pt_trans.at<double>(1);
        double Z = pt_trans.at<double>(2);

        if (Z <= 0.05) continue;

        float u = static_cast<float>(X * intr.fx / Z + intr.cx);
        float v = static_cast<float>(Y * intr.fy / Z + intr.cy);

        if (u >= 0 && u < intr.width && v >= 0 && v < intr.height) {
            projected_2d.emplace_back(u, v);
            sampled_makeup_coords.emplace_back(
                uv_coords_2d[i].x * makeup_texture.cols,
                uv_coords_2d[i].y * makeup_texture.rows
            );
        }
    }

    return apply_makeup_from_uv(sampled_makeup_coords, projected_2d, makeup_texture, alpha_texture, target_image);
}

bool initGLESContext() {
    // EGL context setup for headless Jetson environment
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, nullptr, nullptr)) {
        std::cerr << "Failed to initialize EGL" << std::endl;
        return false;
    }
    return true;
}

void renderMakeupOverlay(
    GLuint vbo,
    GLuint uvbo,
    GLuint makeupTex,
    GLuint alphaTex,
    const float* mvpMatrix,
    int width,
    int height) {

    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint shader = glCreateProgram();
    // ... shader compile/link skipped here for brevity (should be handled externally)

    glUseProgram(shader);

    GLint mvpLoc = glGetUniformLocation(shader, "u_MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvpMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, makeupTex);
    glUniform1i(glGetUniformLocation(shader, "u_makeupTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, alphaTex);
    glUniform1i(glGetUniformLocation(shader, "u_alphaTex"), 1);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvbo);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, /* count */ 3 * 68); // or however many triangles

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);
}

} // namespace makeup
