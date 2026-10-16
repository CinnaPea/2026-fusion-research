//
// Created by hakgu on 10/17/2026.
//

#include "application/application_controller.h"

#include "core/enhancement/n5_thermal_enhancer.h"
#include "core/enhancement/thermal_input.h"

#include <opencv2/core.hpp>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireExact(
    const cv::Mat& actual,
    const cv::Mat& expected,
    const char* message)
{
    require(
        actual.type() == expected.type()
            && actual.size() == expected.size(),
        message
    );

    require(
        cv::norm(actual, expected, cv::NORM_INF) == 0.0,
        message
    );
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

    throw std::runtime_error(
        "Controller accepted an invalid request"
    );
}

} // namespace

int main()
{
    using qart::application::ApplicationController;
    using qart::application::ThermalProcessingMode;
    using qart::core::enhancement::N5ThermalEnhancer;
    using qart::core::enhancement::prepareThermalUint8Plane;

    try {
        ApplicationController controller;
        N5ThermalEnhancer referenceEnhancer;

        for (int channels : {1, 3, 4}) {
            cv::Mat parent(
                23, 29, CV_MAKETYPE(CV_8U, channels)
            );

            cv::RNG random(42 + channels);
            random.fill(
                parent, cv::RNG::UNIFORM, 0, 256
            );

            const cv::Mat source =
                parent(cv::Rect(2, 3, 19, 17));

            const cv::Mat originalParent = parent.clone();
            const cv::Mat prepared =
                prepareThermalUint8Plane(source);

            // No mode argument must mean BYPASS.
            auto defaultResult =
                controller.processThermal(source);

            require(
                defaultResult.appliedMode
                    == ThermalProcessingMode::Bypass,
                "Default mode is not BYPASS"
            );

            requireExact(
                defaultResult.outputImage,
                prepared,
                "Default output differs from prepared input"
            );

            defaultResult.outputImage.setTo(0);

            requireExact(
                parent,
                originalParent,
                "Default result aliases source storage"
            );

            // Explicit N5 must match direct accepted-core processing.
            auto n5Result = controller.processThermal(
                source,
                ThermalProcessingMode::N5PostHalf
            );

            const auto expected =
                referenceEnhancer.enhance(prepared);

            require(
                n5Result.appliedMode
                    == ThermalProcessingMode::N5PostHalf,
                "Explicit N5 mode was not applied"
            );

            requireExact(
                n5Result.outputImage,
                expected.outputImage,
                "Controller N5 differs from direct core N5"
            );

            n5Result.outputImage.setTo(255);

            requireExact(
                parent,
                originalParent,
                "N5 result aliases source storage"
            );

            // A previous N5 call must not change the default.
            const auto laterDefault =
                controller.processThermal(source);

            require(
                laterDefault.appliedMode
                    == ThermalProcessingMode::Bypass,
                "N5 selection leaked into a later default call"
            );

            requireExact(
                laterDefault.outputImage,
                prepared,
                "Later default call did not bypass"
            );
        }

        const cv::Mat constant(
            17, 19, CV_8UC1, cv::Scalar(127)
        );

        const auto constantResult =
            controller.processThermal(
                constant,
                ThermalProcessingMode::N5PostHalf
            );

        requireExact(
            constantResult.outputImage,
            constant,
            "Controller lost constant preservation"
        );

        const cv::Mat empty;
        const cv::Mat highDepth(3, 4, CV_16UC1);
        const cv::Mat wrongChannels(3, 4, CV_8UC2);

        for (const auto& invalid :
             {empty, highDepth, wrongChannels}) {
            requireInvalidInput([&] {
                (void)controller.processThermal(invalid);
            });

            requireInvalidInput([&] {
                (void)controller.processThermal(
                    invalid,
                    ThermalProcessingMode::N5PostHalf
                );
            });
        }

        requireInvalidInput([&] {
            (void)controller.processThermal(
                constant,
                static_cast<ThermalProcessingMode>(999)
            );
        });

        require(
            controller.currentDatasetId().empty()
                && controller.datasetRoot().empty()
                && controller.discoveredPairs().empty()
                && controller.manualSelectionCount() == 0,
            "Thermal processing changed dataset session state"
        );

        std::cout
            << "Application thermal processing contract passed\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}