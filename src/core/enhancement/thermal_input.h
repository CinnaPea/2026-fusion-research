//
// Created by hakgu on 10/15/2026.
//
#pragma once

#include <opencv2/core/mat.hpp>

#ifndef VISUAL_THERMAL_CONCEPT_THERMAL_INPUT_H
#define VISUAL_THERMAL_CONCEPT_THERMAL_INPUT_H

namespace qart::core::enhancement {
    [[nodiscard]]
    cv::Mat prepareThermalUint8Plane(const cv::Mat& img);
    [[nodiscard]]
    cv::Mat bypassThermal(const cv::Mat& plane);
}

#endif //VISUAL_THERMAL_CONCEPT_THERMAL_INPUT_H