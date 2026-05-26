#pragma once

#include "settings/Config.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class IFrameCapture {
public:
    virtual ~IFrameCapture() = default;
    virtual bool open(const AppConfig& cfg) = 0;
    virtual bool read(cv::Mat& rgbFrame) = 0;
    virtual void close() = 0;
};

class RawUdpCapture final : public IFrameCapture {
public:
    RawUdpCapture();
    ~RawUdpCapture() override;

    bool open(const AppConfig& cfg) override;
    bool read(cv::Mat& rgbFrame) override;
    void close() override;

private:
    struct FrameBuffer {
        std::vector<std::vector<uint8_t>> packets;
        int received = 0;
    };

    SOCKET sock_ = INVALID_SOCKET;
    bool wsaReady_ = false;
    int width_ = 128;
    int height_ = 128;
    std::unordered_map<uint32_t, FrameBuffer> buffers_;
};

class ObsCameraCapture final : public IFrameCapture {
public:
    bool open(const AppConfig& cfg) override;
    bool read(cv::Mat& rgbFrame) override;
    void close() override;

private:
    cv::VideoCapture cap_;
    int width_ = 128;
    int height_ = 128;
};

std::unique_ptr<IFrameCapture> makeCapture(const AppConfig& cfg);
