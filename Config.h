#pragma once

#include "settings/Config.h"
#include <opencv2/core.hpp>

class MovementController {
public:
    void reset();
    cv::Point2i compute(const cv::Point2f& target, int frameW, int frameH, const AppConfig& cfg);

private:
    cv::Point2f filtered_{0.0f, 0.0f};
    bool hasFilter_ = false;
};
