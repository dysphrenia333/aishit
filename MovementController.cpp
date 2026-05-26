#pragma once

#include <opencv2/core.hpp>

class KalmanFilter2D {
public:
    KalmanFilter2D() = default;

    void configure(float measurementNoise, float positionProcessNoise, float velocityProcessNoise, float initialP);
    void reset();
    [[nodiscard]] bool initialized() const { return initialized_; }
    [[nodiscard]] cv::Point2f pos() const { return {x_[0], x_[1]}; }
    [[nodiscard]] cv::Point2f vel() const { return {x_[2], x_[3]}; }

    cv::Point2f predict(float dtSeconds);
    cv::Point2f correct(const cv::Point2f& measurement);

private:
    cv::Matx<float, 4, 1> x_{0, 0, 0, 0};
    cv::Matx44f P_ = cv::Matx44f::eye();
    cv::Matx<float, 2, 4> H_{1, 0, 0, 0,
                              0, 1, 0, 0};
    float measurementNoise_ = 1.0f;
    float positionProcessNoise_ = 0.2f;
    float velocityProcessNoise_ = 0.6f;
    float initialP_ = 1.0f;
    bool initialized_ = false;
};
