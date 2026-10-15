#pragma once

#include <opencv2/core/mat.hpp>

namespace qart::core::enhancement {

// Justification & Paper Alignment
// Choice: typed thermal preparation and source-preserving BYPASS functions.
// Classification: Engineering Decision — not paper-derived.
// Rule: mirror Experiment B preparation and BYPASS input/copy semantics;
// no normalization, policy, gain or scientific logic changes are introduced.
// Invalid inputs throw std::invalid_argument. OpenCV failures propagate.
// Both functions return owned CV_8UC1 storage; input is never modified.

// Admit nonempty 2D uint8 matrices with 1, 3 (BGR), or 4 (BGRA) channels.
// Alpha is ignored as in cv::COLOR_BGRA2GRAY. Strided/ROI inputs are supported.
[[nodiscard]] cv::Mat prepareThermalUint8Plane(const cv::Mat& image);

// Admit only an already prepared nonempty 2D CV_8UC1 plane.
[[nodiscard]] cv::Mat bypassThermal(const cv::Mat& plane);

} // namespace qart::core::enhancement
