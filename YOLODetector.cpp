#include "capture/FrameCapture.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace {
constexpr int kMaxPacketPayload = 1400;
constexpr int kHeaderSize = 6; // uint32 frame_id network order, uint8 packet_id, uint8 total_packets

void centerResizeRgb(const cv::Mat& bgrOrRgb, cv::Mat& outRgb, int width, int height, bool inputIsBgr) {
    cv::Mat rgb;
    if (inputIsBgr) {
        cv::cvtColor(bgrOrRgb, rgb, cv::COLOR_BGR2RGB);
    } else {
        rgb = bgrOrRgb;
    }

    const int side = std::min(rgb.cols, rgb.rows);
    const int left = std::max(0, (rgb.cols - side) / 2);
    const int top = std::max(0, (rgb.rows - side) / 2);
    cv::Mat crop = rgb(cv::Rect(left, top, side, side));
    cv::resize(crop, outRgb, cv::Size(width, height), 0.0, 0.0, cv::INTER_LINEAR);
}
}

RawUdpCapture::RawUdpCapture() = default;
RawUdpCapture::~RawUdpCapture() { close(); }

bool RawUdpCapture::open(const AppConfig& cfg) {
    width_ = cfg.frameWidth;
    height_ = cfg.frameHeight;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }
    wsaReady_ = true;

    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.udpPort);
    if (cfg.udpBindIp == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, cfg.udpBindIp.c_str(), &addr.sin_addr);
    }

    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind UDP failed on port " << cfg.udpPort << "\n";
        close();
        return false;
    }

    DWORD timeoutMs = 20;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    buffers_.clear();
    return true;
}

bool RawUdpCapture::read(cv::Mat& rgbFrame) {
    if (sock_ == INVALID_SOCKET) {
        return false;
    }

    uint8_t packet[kHeaderSize + kMaxPacketPayload]{};
    sockaddr_in from{};
    int fromLen = sizeof(from);
    const int bytes = recvfrom(sock_, reinterpret_cast<char*>(packet), sizeof(packet), 0,
                               reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (bytes < kHeaderSize) {
        return false;
    }

    uint32_t frameIdNetwork = 0;
    std::memcpy(&frameIdNetwork, packet, sizeof(uint32_t));
    const uint32_t frameId = ntohl(frameIdNetwork);
    const uint8_t packetId = packet[4];
    const uint8_t totalPackets = packet[5];
    if (totalPackets == 0 || packetId >= totalPackets) {
        return false;
    }

    auto& fb = buffers_[frameId];
    if (fb.packets.empty()) {
        fb.packets.resize(totalPackets);
    }
    if (packetId >= fb.packets.size()) {
        buffers_.erase(frameId);
        return false;
    }
    if (fb.packets[packetId].empty()) {
        fb.received++;
    }
    fb.packets[packetId].assign(packet + kHeaderSize, packet + bytes);

    if (fb.received != static_cast<int>(fb.packets.size())) {
        if (buffers_.size() > 8) {
            auto oldest = std::min_element(buffers_.begin(), buffers_.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            if (oldest != buffers_.end()) {
                buffers_.erase(oldest);
            }
        }
        return false;
    }

    std::vector<uint8_t> full;
    for (const auto& p : fb.packets) {
        full.insert(full.end(), p.begin(), p.end());
    }
    buffers_.erase(frameId);

    const size_t expectedRgba = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4;
    const size_t expectedRgb = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3;
    if (full.size() == expectedRgba) {
        cv::Mat rgba(height_, width_, CV_8UC4, full.data());
        cv::cvtColor(rgba, rgbFrame, cv::COLOR_RGBA2RGB);
        return true;
    }
    if (full.size() == expectedRgb) {
        cv::Mat rgb(height_, width_, CV_8UC3, full.data());
        rgbFrame = rgb.clone();
        return true;
    }
    return false;
}

void RawUdpCapture::close() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    if (wsaReady_) {
        WSACleanup();
        wsaReady_ = false;
    }
    buffers_.clear();
}

bool ObsCameraCapture::open(const AppConfig& cfg) {
    width_ = cfg.frameWidth;
    height_ = cfg.frameHeight;
    if (!cap_.open(cfg.obsCameraIndex, cv::CAP_DSHOW)) {
        if (!cap_.open(cfg.obsCameraIndex)) {
            std::cerr << "Could not open OBS virtual camera index " << cfg.obsCameraIndex << "\n";
            return false;
        }
    }
    return true;
}

bool ObsCameraCapture::read(cv::Mat& rgbFrame) {
    cv::Mat bgr;
    if (!cap_.read(bgr) || bgr.empty()) {
        return false;
    }
    centerResizeRgb(bgr, rgbFrame, width_, height_, true);
    return true;
}

void ObsCameraCapture::close() {
    cap_.release();
}

std::unique_ptr<IFrameCapture> makeCapture(const AppConfig& cfg) {
    if (cfg.captureMode == "OBS") {
        return std::make_unique<ObsCameraCapture>();
    }
    return std::make_unique<RawUdpCapture>();
}
