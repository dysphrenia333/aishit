#include "tracking/KalmanFilter2D.h"

#include <algorithm>

void KalmanFilter2D::configure(float measurementNoise, float positionProcessNoise, float velocityProcessNoise, float initialP) {
    measurementNoise_ = measurementNoise;
    positionProcessNoise_ = positionProcessNoise;
    velocityProcessNoise_ = velocityProcessNoise;
    initialP_ = initialP;
    reset();
}

void KalmanFilter2D::reset() {
    x_ = cv::Matx<float, 4, 1>(0, 0, 0, 0);
    P_ = cv::Matx44f::eye() * initialP_;
    initialized_ = false;
}

cv::Point2f KalmanFilter2D::predict(float dtSeconds) {
    if (!initialized_) {
        return pos();
    }
    const float dt = std::clamp(dtSeconds, 0.0001f, 0.25f);
    cv::Matx44f F = cv::Matx44f::eye();
    F(0, 2) = dt;
    F(1, 3) = dt;

    cv::Matx44f Q = cv::Matx44f::zeros();
    Q(0, 0) = positionProcessNoise_ * dt;
    Q(1, 1) = positionProcessNoise_ * dt;
    Q(2, 2) = velocityProcessNoise_;
    Q(3, 3) = velocityProcessNoise_;

    x_ = F * x_;
    P_ = F * P_ * F.t() + Q;
    return pos();
}

cv::Point2f KalmanFilter2D::correct(const cv::Point2f& measurement) {
    if (!initialized_) {
        x_ = cv::Matx<float, 4, 1>(measurement.x, measurement.y, 0, 0);
        P_ = cv::Matx44f::eye() * initialP_;
        initialized_ = true;
        return pos();
    }

    cv::Matx<float, 2, 1> z(measurement.x, measurement.y);
    cv::Matx<float, 2, 1> y = z - H_ * x_;
    cv::Matx22f R = cv::Matx22f::eye() * measurementNoise_;
    cv::Matx22f S = H_ * P_ * H_.t() + R;
    cv::Matx22f SInv = S.inv(cv::DECOMP_SVD);
    cv::Matx<float, 4, 2> K = P_ * H_.t() * SInv;

    x_ = x_ + K * y;
    cv::Matx44f I = cv::Matx44f::eye();
    P_ = (I - K * H_) * P_ * (I - K * H_).t() + K * R * K.t();
    return pos();
}
