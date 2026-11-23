//
// Created by hakgu on 10/16/2026.
//

#include "thermal_input.h"
#include <opencv5/opencv2/imgproc.hpp>
#include <stdexcept>

namespace qart::core::enhancement {
    namespace {

        void validateUint8ImageGeometry(const cv::Mat& image)
        {
            // Applicable Python validation order:
            // empty input, dtype, then dimensionality.
            if (image.empty()) {
                throw std::invalid_argument(
                    "thermal input must not be empty"
                );
            }

            if (image.depth() != CV_8U) {
                throw std::invalid_argument(
                    "thermal input must use uint8"
                );
            }

            if (image.dims != 2) {
                throw std::invalid_argument(
                    "thermal input must have 2D image geometry"
                );
            }
        }

    } // namespace

    cv::Mat prepareThermalUint8Plane(const cv::Mat& image)
    {
        validateUint8ImageGeometry(image);

        switch (image.channels()) {
            case 1:
                return image.clone();

            case 3: {
                cv::Mat output;
                cv::cvtColor(image, output, cv::COLOR_BGR2GRAY);
                return output;
            }

            case 4: {
                cv::Mat output;
                cv::cvtColor(image, output, cv::COLOR_BGRA2GRAY);
                return output;
            }

            default:
                throw std::invalid_argument(
                    "thermal input must have 1, 3 or 4 channels"
                );
        }
    }

    cv::Mat bypassThermal(const cv::Mat& plane)
    {
        validateUint8ImageGeometry(plane);

        if (plane.channels() != 1) {
            throw std::invalid_argument(
                "BYPASS requires a single-channel thermal plane"
            );
        }

        return plane.clone();
    }

} // namespace qart::core::enhancement
