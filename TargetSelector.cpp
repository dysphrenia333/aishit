#include "detector/YOLODetector.h"

#include <opencv2/imgproc.hpp>

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <thread>

namespace {
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), needed);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}
}

YOLODetector::YOLODetector()
    : env_(ORT_LOGGING_LEVEL_WARNING, "CppAimMinimal"),
      memoryInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    sessionOptions_.SetIntraOpNumThreads(std::max(1u, std::thread::hardware_concurrency()));
    sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

bool YOLODetector::load(const std::string& modelPath, int fallbackInputW, int fallbackInputH) {
    try {
        const std::wstring widePath = utf8ToWide(modelPath);
        session_ = std::make_unique<Ort::Session>(env_, widePath.c_str(), sessionOptions_);

        Ort::AllocatorWithDefaultOptions allocator;
        auto inputNameAlloc = session_->GetInputNameAllocated(0, allocator);
        auto outputNameAlloc = session_->GetOutputNameAllocated(0, allocator);
        inputName_ = inputNameAlloc.get();
        outputName_ = outputNameAlloc.get();

        auto inputInfo = session_->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        auto inputShape = inputInfo.GetShape();
        if (inputShape.size() == 4) {
            const int64_t h = inputShape[2];
            const int64_t w = inputShape[3];
            inputH_ = h > 0 ? static_cast<int>(h) : fallbackInputH;
            inputW_ = w > 0 ? static_cast<int>(w) : fallbackInputW;
        } else {
            inputW_ = fallbackInputW;
            inputH_ = fallbackInputH;
        }

        loaded_ = true;
        std::cout << "Loaded ONNX model: " << modelPath << " input=" << inputW_ << "x" << inputH_ << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "ONNX load failed: " << e.what() << "\n";
        session_.reset();
        loaded_ = false;
        return false;
    }
}

std::vector<float> YOLODetector::preprocess(const cv::Mat& rgbFrame) {
    cv::Mat resized;
    cv::resize(rgbFrame, resized, cv::Size(inputW_, inputH_), 0.0, 0.0, cv::INTER_LINEAR);
    resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);

    std::vector<float> tensor(static_cast<size_t>(3 * inputW_ * inputH_));
    const int plane = inputW_ * inputH_;
    for (int y = 0; y < inputH_; ++y) {
        const auto* row = resized.ptr<cv::Vec3f>(y);
        for (int x = 0; x < inputW_; ++x) {
            const cv::Vec3f& px = row[x];
            tensor[0 * plane + y * inputW_ + x] = px[0];
            tensor[1 * plane + y * inputW_ + x] = px[1];
            tensor[2 * plane + y * inputW_ + x] = px[2];
        }
    }
    return tensor;
}

std::vector<BBox> YOLODetector::detect(const cv::Mat& rgbFrame, float confThreshold, float nmsThreshold) {
    if (!loaded_ || rgbFrame.empty()) {
        return {};
    }

    auto inputTensorValues = preprocess(rgbFrame);
    std::array<int64_t, 4> inputShape{1, 3, inputH_, inputW_};
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo_, inputTensorValues.data(), inputTensorValues.size(), inputShape.data(), inputShape.size());

    const char* inputNames[] = {inputName_.c_str()};
    const char* outputNames[] = {outputName_.c_str()};

    try {
        auto outputs = session_->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        auto& output = outputs.front();
        auto info = output.GetTensorTypeAndShapeInfo();
        auto shape = info.GetShape();
        const float* raw = output.GetTensorData<float>();
        const size_t count = info.GetElementCount();
        std::vector<float> out(raw, raw + count);
        return postprocess(out, shape, rgbFrame.cols, rgbFrame.rows, confThreshold, nmsThreshold);
    } catch (const std::exception& e) {
        std::cerr << "ONNX inference failed: " << e.what() << "\n";
        return {};
    }
}

std::vector<BBox> YOLODetector::postprocess(const std::vector<float>& output, const std::vector<int64_t>& shape,
                                            int frameW, int frameH, float confThreshold, float nmsThreshold) const {
    std::vector<BBox> boxes;
    if (shape.size() != 3 || output.empty()) {
        return boxes;
    }

    int64_t dim1 = shape[1];
    int64_t dim2 = shape[2];
    bool transposed = false;

    // YOLOv8 common formats: [1, 84, N] or [1, N, 84].
    int64_t rows = 0;
    int64_t cols = 0;
    if (dim1 < dim2) {
        transposed = true;
        cols = dim1;
        rows = dim2;
    } else {
        transposed = false;
        rows = dim1;
        cols = dim2;
    }
    if (cols < 5) {
        return boxes;
    }

    auto at = [&](int64_t r, int64_t c) -> float {
        if (transposed) {
            return output[static_cast<size_t>(c * rows + r)];
        }
        return output[static_cast<size_t>(r * cols + c)];
    };

    const float scaleX = static_cast<float>(frameW) / static_cast<float>(inputW_);
    const float scaleY = static_cast<float>(frameH) / static_cast<float>(inputH_);

    for (int64_t r = 0; r < rows; ++r) {
        const float cx = at(r, 0);
        const float cy = at(r, 1);
        const float w = at(r, 2);
        const float h = at(r, 3);

        float bestScore = 0.0f;
        int bestClass = 0;
        for (int64_t c = 4; c < cols; ++c) {
            const float score = at(r, c);
            if (score > bestScore) {
                bestScore = score;
                bestClass = static_cast<int>(c - 4);
            }
        }
        if (bestScore < confThreshold) {
            continue;
        }

        BBox b;
        b.x1 = (cx - w * 0.5f) * scaleX;
        b.y1 = (cy - h * 0.5f) * scaleY;
        b.x2 = (cx + w * 0.5f) * scaleX;
        b.y2 = (cy + h * 0.5f) * scaleY;
        b.x1 = std::clamp(b.x1, 0.0f, static_cast<float>(frameW - 1));
        b.y1 = std::clamp(b.y1, 0.0f, static_cast<float>(frameH - 1));
        b.x2 = std::clamp(b.x2, 0.0f, static_cast<float>(frameW - 1));
        b.y2 = std::clamp(b.y2, 0.0f, static_cast<float>(frameH - 1));
        b.confidence = bestScore;
        b.classId = bestClass;
        if (b.width() > 1.0f && b.height() > 1.0f) {
            boxes.push_back(b);
        }
    }

    nms(boxes, nmsThreshold);
    return boxes;
}

float YOLODetector::iou(const BBox& a, const BBox& b) {
    const float xx1 = std::max(a.x1, b.x1);
    const float yy1 = std::max(a.y1, b.y1);
    const float xx2 = std::min(a.x2, b.x2);
    const float yy2 = std::min(a.y2, b.y2);
    const float w = std::max(0.0f, xx2 - xx1);
    const float h = std::max(0.0f, yy2 - yy1);
    const float inter = w * h;
    const float areaA = std::max(0.0f, a.width()) * std::max(0.0f, a.height());
    const float areaB = std::max(0.0f, b.width()) * std::max(0.0f, b.height());
    const float denom = areaA + areaB - inter;
    return denom > 1e-6f ? inter / denom : 0.0f;
}

void YOLODetector::nms(std::vector<BBox>& boxes, float threshold) {
    std::sort(boxes.begin(), boxes.end(), [](const BBox& a, const BBox& b) {
        return a.confidence > b.confidence;
    });

    std::vector<BBox> kept;
    std::vector<bool> removed(boxes.size(), false);
    for (size_t i = 0; i < boxes.size(); ++i) {
        if (removed[i]) {
            continue;
        }
        kept.push_back(boxes[i]);
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            if (!removed[j] && boxes[i].classId == boxes[j].classId && iou(boxes[i], boxes[j]) > threshold) {
                removed[j] = true;
            }
        }
    }
    boxes.swap(kept);
}
