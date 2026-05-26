#pragma once

#include "settings/Config.h"
#include <opencv2/core.hpp>
#include <string>

class MouseController {
public:
    bool open(const AppConfig& cfg);
    void close();
    void moveRelative(int dx, int dy);

private:
    std::string mode_ = "WinApi";
};
