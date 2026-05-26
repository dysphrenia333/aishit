#include "tracking/TargetSelector.h"

#include <limits>

std::optional<BBox> TargetSelector::chooseClosestToCenter(const std::vector<BBox>& boxes, int frameW, int frameH) {
    if (boxes.empty()) {
        return std::nullopt;
    }
    const cv::Point2f center(frameW * 0.5f, frameH * 0.5f);
    float bestDist2 = std::numeric_limits<float>::max();
    std::optional<BBox> best;
    for (const auto& b : boxes) {
        const cv::Point2f c = b.center();
        const float dx = c.x - center.x;
        const float dy = c.y - center.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist2) {
            bestDist2 = d2;
            best = b;
        }
    }
    return best;
}
