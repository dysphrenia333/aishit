#include "control/MovementController.h"

#include <algorithm>
#include <cmath>

void MovementController::reset() {
    filtered_ = {0.0f, 0.0f};
    hasFilter_ = false;
}

cv::Point2i MovementController::compute(const cv::Point2f& target, int frameW, int frameH, const AppConfig& cfg) {
    const cv::Point2f center(frameW * 0.5f, frameH * 0.5f);
    cv::Point2f err(target.x - center.x, target.y - center.y);

    if (std::abs(err.x) < cfg.deadzone) {
        err.x = 0.0f;
    }
    if (std::abs(err.y) < cfg.deadzone) {
        err.y = 0.0f;
    }
    if (err.x == 0.0f && err.y == 0.0f) {
        reset();
        return {0, 0};
    }

    cv::Point2f raw(err.x * cfg.sensitivityX, err.y * cfg.sensitivityY);

    const float alpha = cfg.smoothing <= 0.0f ? 1.0f : 1.0f / (1.0f + cfg.smoothing * 4.0f);
    if (!hasFilter_) {
        filtered_ = raw;
        hasFilter_ = true;
    } else {
        filtered_.x = alpha * raw.x + (1.0f - alpha) * filtered_.x;
        filtered_.y = alpha * raw.y + (1.0f - alpha) * filtered_.y;
    }

    filtered_.x = std::clamp(filtered_.x, -cfg.maxSpeed, cfg.maxSpeed);
    filtered_.y = std::clamp(filtered_.y, -cfg.maxSpeed, cfg.maxSpeed);

    return {static_cast<int>(std::round(filtered_.x)), static_cast<int>(std::round(filtered_.y))};
}
