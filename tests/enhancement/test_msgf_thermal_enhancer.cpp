#include "core/enhancement/msgf_thermal_enhancer.h"

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
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("MSGF accepted invalid input");
}

} // namespace

int main()
{
    using qart::core::enhancement::MsgfParameters;
    using qart::core::enhancement::MsgfThermalEnhancer;

    try {
        requireInvalidInput([] {
            MsgfParameters p;
            p.radii.clear();
            (void)MsgfThermalEnhancer(p);
        });
        requireInvalidInput([] {
            MsgfParameters p;
            p.radii = {3, 0};
            (void)MsgfThermalEnhancer(p);
        });
        requireInvalidInput([] {
            MsgfParameters p;
            p.baseClaheTiles = 0;
            (void)MsgfThermalEnhancer(p);
        });

        MsgfThermalEnhancer enhancer;

        for (const cv::Size size : {
                 cv::Size(1, 1),
                 cv::Size(17, 1),
                 cv::Size(1, 19),
                 cv::Size(64, 48)
             }) {
            cv::Mat source(size, CV_8UC1);
            cv::RNG random(101);
            random.fill(source, cv::RNG::UNIFORM, 20, 220);

            const cv::Mat original = source.clone();
            auto first = enhancer.enhance(source);
            auto second = enhancer.enhance(source);

            require(first.outputImage.type() == CV_8UC1
                        && first.outputImage.size() == source.size(),
                    "MSGF output type or geometry mismatch");
            require(first.outputImage.data != source.data,
                    "MSGF output aliases input storage");
            require(first.outputImage.data != second.outputImage.data,
                    "MSGF repeated calls share output storage");
            require(cv::norm(source, original, cv::NORM_INF) == 0.0,
                    "MSGF mutated input image");
            require(cv::norm(first.outputImage, second.outputImage, cv::NORM_INF) == 0.0,
                    "MSGF result not deterministic");
        }

        // Rich gradient/hotspot pattern should be enhanced visibly (different from raw input).
        cv::Mat pattern(64, 64, CV_8UC1);
        for (int y = 0; y < pattern.rows; ++y) {
            for (int x = 0; x < pattern.cols; ++x) {
                pattern.at<unsigned char>(y, x) = static_cast<unsigned char>(
                    40 + (x + y) % 80
                );
            }
        }
        const auto patternOut = enhancer.enhance(pattern);
        require(cv::countNonZero(patternOut.outputImage != pattern) > 0,
                "MSGF produced no change on textured image");

        requireInvalidInput([&] {
            (void)enhancer.enhance(cv::Mat());
        });
        requireInvalidInput([&] {
            (void)enhancer.enhance(cv::Mat(8, 8, CV_16UC1));
        });
        requireInvalidInput([&] {
            (void)enhancer.enhance(cv::Mat(8, 8, CV_8UC3));
        });

        std::cout << "MSGF thermal enhancer contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
