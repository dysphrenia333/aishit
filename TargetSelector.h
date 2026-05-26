#pragma once

#include "detector/BBox.h"
#include "settings/Config.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>

#include <memory>
#include <string>
#include <vector>

class YOLODetector {
public:
    YOLODetector();
    ~YOLODetector() = default;

    bool load(const std::string& modelPath, int fallbackInputW = 128, int fallbackInputH = 128);
    [[nodiscard]] bool isLoaded() const { return loaded_; }
    std::vector<BBox> detect(const cv::Mat& rgbFrame, float confThreshold, float nmsThreshold);

private:
    std::vector<float> preprocess(const cv::Mat& rgbFrame);
    std::vector<BBox> postprocess(const std::vector<float>& output, const std::vector<int64_t>& shape,
                                  int frameW, int frameH, float confThreshold, float nmsThreshold) const;
    static float iou(const BBox& a, const BBox& b);
    static void nms(std::vector<BBox>& boxes, float threshold);

    Ort::Env env_;
    Ort::SessionOptions sessionOptions_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memoryInfo_;
    std::string inputName_;
    std::string outputName_;
    int inputW_ = 128;
    int inputH_ = 128;
    bool loaded_ = false;
};
