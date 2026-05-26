#pragma once

#include <opencv2/core.hpp>

struct BBox {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float confidence = 0.0f;
    int classId = -1;

    [[nodiscard]] cv::Point2f center() const {
        return { (x1 + x2) * 0.5f, (y1 + y2) * 0.5f };
    }

    [[nodiscard]] float width() const { return x2 - x1; }
    [[nodiscard]] float height() const { return y2 - y1; }
};
