//
// Created by hakgu on 10/16/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_N5_THERMAL_ENHANCER_H
#define VISUAL_THERMAL_CONCEPT_N5_THERMAL_ENHANCER_H

#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace qart::core::enhancement {

    struct N5EnhancementResult
    {
        cv::Mat outputImage;
        double smallRegularization;
        bool constantPreserved;
    };

    // Reusable processor. Do not call one instance concurrently.
    // Give each processing thread its own instance.
    class N5ThermalEnhancer final
    {
    public:
        N5ThermalEnhancer();

        N5ThermalEnhancer(const N5ThermalEnhancer&) = delete;
        N5ThermalEnhancer& operator=(
            const N5ThermalEnhancer&) = delete;

        N5ThermalEnhancer(N5ThermalEnhancer&&) = delete;
        N5ThermalEnhancer& operator=(
            N5ThermalEnhancer&&) = delete;

        // Input: nonempty 2D CV_8UC1 thermal plane.
        // Output: independently owned CV_8UC1, same geometry.
        // Input is never modified.
        [[nodiscard]]
        N5EnhancementResult enhance(const cv::Mat& plane);

    private:
        cv::Ptr<cv::CLAHE> clahe_;
    };

} // namespace qart::core::enhancement

#endif //VISUAL_THERMAL_CONCEPT_N5_THERMAL_ENHANCER_H