// Justification & Paper Alignment
// Choice: retain OpenCV's color conversion and clone at copy boundaries.
// Classification: Engineering Decision — not paper-derived.
// Rule: exact translation of experiment_b.prepare_thermal_uint8_plane and
// baselines.BypassThermalEnhancer; owned results protect source imagery.
// This infrastructure does not alter scientific logic or select enhancement.
#include "thermal_input.h"

#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace qart::core::enhancement {
namespace {
void validateUint8PlaneGeometry(const cv::Mat& image)
{
    // Same applicable precedence as Python: empty, dtype, dimensionality.
    if (image.empty()) {
        throw std::invalid_argument("thermal input must not be empty");
    }
    if (image.depth() != CV_8U) {
        throw std::invalid_argument("thermal input must use uint8");
    }
    if (image.dims != 2) {
        throw std::invalid_argument("thermal input must have 2D image geometry");
    }
}
} // namespace

cv::Mat prepareThermalUint8Plane(const cv::Mat& image)
{
    validateUint8PlaneGeometry(image);
    switch (image.channels()) {
    case 1:
        return image.clone();
    case 3: {
        cv::Mat result;
        cv::cvtColor(image, result, cv::COLOR_BGR2GRAY);
        return result;
    }
    case 4: {
        cv::Mat result;
        cv::cvtColor(image, result, cv::COLOR_BGRA2GRAY);
        return result;
    }
    default:
        throw std::invalid_argument("thermal input must have 1, 3 or 4 channels");
    }
}

cv::Mat bypassThermal(const cv::Mat& plane)
{
    validateUint8PlaneGeometry(plane);
    if (plane.channels() != 1) {
        throw std::invalid_argument("BYPASS requires a single-channel thermal plane");
    }
    return plane.clone();
}
} // namespace qart::core::enhancement
