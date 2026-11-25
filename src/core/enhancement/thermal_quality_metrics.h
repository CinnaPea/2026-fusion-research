#ifndef VISUAL_THERMAL_CONCEPT_THERMAL_QUALITY_METRICS_H
#define VISUAL_THERMAL_CONCEPT_THERMAL_QUALITY_METRICS_H

#pragma once

#include <opencv2/core.hpp>

namespace qart::core::enhancement {

struct PlaneQualityStats {
    bool valid = false;
    double mean = 0.0;
    double stdDev = 0.0;
    double informationEntropy = 0.0;
    double averageGradient = 0.0;
    double piqe = 0.0;
    int minVal = 0;
    int maxVal = 0;
};

struct PairQualityMetrics {
    bool valid = false;
    PlaneQualityStats input;
    PlaneQualityStats output;
    double psnr = 0.0;
    double mae = 0.0;
    int maxAbsDiff = 0;
    double changedPixelPercent = 0.0;
};

[[nodiscard]]
PlaneQualityStats computePlaneQualityStats(const cv::Mat& gray8);

[[nodiscard]]
PairQualityMetrics computePairQualityMetrics(
    const cv::Mat& inputGray8,
    const cv::Mat& outputGray8
);

} // namespace qart::core::enhancement

#endif // VISUAL_THERMAL_CONCEPT_THERMAL_QUALITY_METRICS_H
