#include "output/MouseController.h"

#include <windows.h>
#include <algorithm>

bool MouseController::open(const AppConfig& cfg) {
    mode_ = cfg.outputMode;
    return true;
}

void MouseController::close() {}

void MouseController::moveRelative(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }
    dx = std::clamp(dx, -127, 127);
    dy = std::clamp(dy, -127, 127);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}
