#pragma once

#include "detector/BBox.h"
#include <optional>
#include <vector>

class TargetSelector {
public:
    static std::optional<BBox> chooseClosestToCenter(const std::vector<BBox>& boxes, int frameW, int frameH);
};
