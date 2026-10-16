//
// Created by hakgu on 10/16/2026.
//

// Justification & Paper Alignment
// Choice: verify conversion, input rejection and storage ownership.
// Classification: Engineering Decision — not paper-derived.
// Rule: test the accepted thermal input/BYPASS contracts.
// These checks do not make image-quality claims.

#include "core/enhancement/thermal_input.h"

#include <opencv5/opencv2/core.hpp>

#include <iostream>
#include <stdexcept>
#include <opencv5/opencv2/core/mat.hpp>
#include <opencv5/opencv2/core/types.hpp>
#include <opencv5/opencv2/core/hal/interface.h>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template<typename Operation>
void requireInvalidInput(Operation operation)
{
    try {
        operation();
    }
    catch (const std::invalid_argument&) {
        return;
    }

    throw std::runtime_error("unsupported input was accepted");
}

void verifyOwnership(
    const cv::Mat& source,
    const cv::Mat& original,
    cv::Mat output)
{
    require(
        output.type() == CV_8UC1
            && output.size() == source.size(),
        "incorrect output type or geometry"
    );

    require(
        output.data != source.data,
        "output aliases source storage"
    );

    require(
        cv::norm(source, original, cv::NORM_INF) == 0.0,
        "operation modified the source"
    );

    output.setTo(17);

    require(
        cv::norm(source, original, cv::NORM_INF) == 0.0,
        "output mutation modified the source"
    );
}

} // namespace

int main()
{
    using namespace qart::core::enhancement;

    try {
        // Verify strided ROI support and ownership.
        for (int channels : {1, 3, 4}) {
            cv::Mat parent(
                9, 13, CV_MAKETYPE(CV_8U, channels)
            );

            cv::RNG random(42);
            random.fill(
                parent, cv::RNG::UNIFORM, 0, 256
            );

            const cv::Mat region =
                parent(cv::Rect(2, 1, 7, 5));

            require(
                !region.isContinuous(),
                "test fixture must be strided"
            );

            const cv::Mat original = region.clone();
            cv::Mat prepared =
                prepareThermalUint8Plane(region);

            require(
                cv::norm(
                    prepared,
                    prepareThermalUint8Plane(original),
                    cv::NORM_INF
                ) == 0.0,
                "ROI conversion differs from owned input"
            );

            if (channels == 1) {
                require(
                    cv::norm(
                        prepared, original, cv::NORM_INF
                    ) == 0.0,
                    "grayscale preparation changed pixels"
                );

                verifyOwnership(
                    region,
                    original,
                    bypassThermal(region)
                );
            }

            verifyOwnership(region, original, prepared);
        }

        // Constants, including endpoint values.
        for (int level : {0, 1, 127, 255}) {
            const cv::Mat input(
                1, 1, CV_8UC1, cv::Scalar(level)
            );

            cv::Mat output = bypassThermal(input);

            require(
                output.at<unsigned char>(0, 0) == level,
                "BYPASS changed a constant"
            );

            verifyOwnership(
                input, input.clone(), output
            );
        }

        // Confirm BGR rather than RGB ordering.
        cv::Mat bgr(1, 3, CV_8UC3);
        bgr.at<cv::Vec3b>(0, 0) = {255, 0, 0};
        bgr.at<cv::Vec3b>(0, 1) = {0, 255, 0};
        bgr.at<cv::Vec3b>(0, 2) = {0, 0, 255};

        const cv::Mat gray =
            prepareThermalUint8Plane(bgr);

        require(
            gray.at<unsigned char>(0, 0) == 29
                && gray.at<unsigned char>(0, 1) == 150
                && gray.at<unsigned char>(0, 2) == 76,
            "incorrect BGR conversion"
        );

        // Alpha must not affect thermal intensity.
        cv::Mat bgra(1, 2, CV_8UC4);
        bgra.at<cv::Vec4b>(0, 0) = {12, 34, 56, 0};
        bgra.at<cv::Vec4b>(0, 1) = {12, 34, 56, 255};

        const cv::Mat alphaIgnored =
            prepareThermalUint8Plane(bgra);

        require(
            alphaIgnored.at<unsigned char>(0, 0)
                == alphaIgnored.at<unsigned char>(0, 1),
            "alpha affected grayscale intensity"
        );

        const cv::Mat empty;
        const cv::Mat wrongDepth(2, 3, CV_16UC1);
        const cv::Mat wrongChannels(2, 3, CV_8UC2);

        const int sizes[] = {2, 3, 4};
        const cv::Mat volume(3, sizes, CV_8UC1);

        for (const auto& invalid :
             {empty, wrongDepth, wrongChannels, volume}) {
            requireInvalidInput([&] {
                (void)prepareThermalUint8Plane(invalid);
            });

            requireInvalidInput([&] {
                (void)bypassThermal(invalid);
            });
        }

        requireInvalidInput([&] {
            (void)bypassThermal(bgr);
        });

        requireInvalidInput([&] {
            (void)bypassThermal(bgra);
        });

        std::cout
            << "Thermal input/BYPASS contracts passed\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}