//
// Created by hakgu on 10/16/2026.
//

#include "core/enhancement/n5_thermal_enhancer.h"

#include <opencv2/core.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

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

    throw std::runtime_error("N5 accepted unsupported input");
}

} // namespace

int main()
{
    using qart::core::enhancement::N5ThermalEnhancer;

    try {
        N5ThermalEnhancer enhancer;

        for (int level : {0, 1, 127, 255}) {
            const cv::Mat source(
                17, 19, CV_8UC1, cv::Scalar(level)
            );

            auto result = enhancer.enhance(source);

            require(result.constantPreserved, "constant flag");
            require(
                cv::norm(
                    source, result.outputImage, cv::NORM_INF
                ) == 0.0,
                "constant pixels changed"
            );
            require(
                result.outputImage.data != source.data,
                "constant output aliases input"
            );
        }

        for (const cv::Size size : {
                 cv::Size(1, 1),
                 cv::Size(17, 1),
                 cv::Size(1, 19),
                 cv::Size(33, 31)
             }) {
            cv::Mat source(size, CV_8UC1);
            cv::RNG random(42);
            random.fill(source, cv::RNG::UNIFORM, 0, 256);

            const cv::Mat original = source.clone();

            auto first = enhancer.enhance(source);
            auto second = enhancer.enhance(source);

            require(
                first.outputImage.type() == CV_8UC1
                    && first.outputImage.size() == source.size(),
                "output type or geometry"
            );
            require(
                first.outputImage.data != source.data,
                "output aliases source"
            );
            require(
                first.outputImage.data != second.outputImage.data,
                "successive outputs share storage"
            );
            require(
                cv::norm(
                    first.outputImage,
                    second.outputImage,
                    cv::NORM_INF
                ) == 0.0,
                "repeated call changed pixels"
            );
            require(
                std::isfinite(first.smallRegularization)
                    && first.smallRegularization >= 0.0,
                "invalid regularization"
            );

            first.outputImage.setTo(23);

            require(
                cv::norm(source, original, cv::NORM_INF) == 0.0,
                "source was modified"
            );
        }

        cv::Mat parent(21, 25, CV_8UC1);
        cv::RNG random(123);
        random.fill(parent, cv::RNG::UNIFORM, 0, 256);

        const cv::Mat region =
            parent(cv::Rect(2, 3, 17, 13));

        const cv::Mat originalParent = parent.clone();

        const auto regionResult = enhancer.enhance(region);
        const auto ownedResult = enhancer.enhance(region.clone());

        require(
            cv::norm(
                regionResult.outputImage,
                ownedResult.outputImage,
                cv::NORM_INF
            ) == 0.0,
            "ROI result depends on parent storage"
        );

        require(
            cv::norm(
                parent, originalParent, cv::NORM_INF
            ) == 0.0,
            "parent image modified"
        );

        const cv::Mat empty;
        const cv::Mat color(3, 4, CV_8UC3);
        const cv::Mat highDepth(3, 4, CV_16UC1);
        const cv::Mat floating(3, 4, CV_64FC1);

        const int dimensions[] = {2, 3, 4};
        const cv::Mat volume(3, dimensions, CV_8UC1);

        for (const auto& invalid :
             {empty, color, highDepth, floating, volume}) {
            requireInvalidInput([&] {
                (void)enhancer.enhance(invalid);
            });
        }

        std::cout << "N5 thermal contract passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}