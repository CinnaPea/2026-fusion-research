#include "core/enhancement/thermal_quality_metrics.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

cv::Mat buildNaturalLikeThermalTestImage(int rows, int cols)
{
    cv::Mat img(rows, cols, CV_8UC1);
    cv::RNG rng(2026);
    cv::Mat microTexture(rows, cols, CV_64F);
    rng.fill(microTexture, cv::RNG::NORMAL, 0.0, 12.0);
    // Spatially correlate fine grain slightly so it mimics crisp natural sensor texture
    // rather than pure i.i.d. AWGN.
    cv::GaussianBlur(microTexture, microTexture, cv::Size(3, 3), 0.75);

    for (int y = 0; y < rows; ++y) {
        auto* rowPtr = img.ptr<unsigned char>(y);
        const auto* texPtr = microTexture.ptr<double>(y);
        for (int x = 0; x < cols; ++x) {
            const double bg = 55.0 + 35.0 * (static_cast<double>(y) / std::max(1, rows - 1));
            const double structure =
                20.0 * std::sin(0.11 * x + 0.05 * y)
                + 14.0 * std::cos(0.45 * y - 0.35 * x)
                + 9.0 * std::sin(0.85 * x + 0.65 * y);
            double hotspot = 0.0;
            const double dx = static_cast<double>(x) - cols * 0.55;
            const double dy = static_cast<double>(y) - rows * 0.45;
            if (std::abs(dx) < cols * 0.14 && std::abs(dy) < rows * 0.18) {
                hotspot = 75.0;
            }
            const double val = std::clamp(bg + structure + hotspot + texPtr[x], 0.0, 255.0);
            rowPtr[x] = static_cast<unsigned char>(std::round(val));
        }
    }
    return img;
}

} // namespace

int main()
{
    using qart::core::enhancement::computePairQualityMetrics;
    using qart::core::enhancement::computePlaneQualityStats;

    try {
        // 1. Invalid inputs return valid=false.
        require(!computePlaneQualityStats(cv::Mat()).valid, "empty plane accepted");
        require(!computePlaneQualityStats(cv::Mat(16, 16, CV_16UC1)).valid, "16U plane accepted");
        require(!computePlaneQualityStats(cv::Mat(16, 16, CV_8UC3)).valid, "3-channel plane accepted");
        require(
            !computePairQualityMetrics(
                 cv::Mat(16, 16, CV_8UC1, cv::Scalar(10)),
                 cv::Mat(16, 20, CV_8UC1, cv::Scalar(10)))
                 .valid,
            "mismatched size accepted"
        );

        // 2. Constant image: Mean=val, Std=0, IE=0, AG=0, PIQE=100 (zero activity).
        const cv::Mat constant(64, 64, CV_8UC1, cv::Scalar(77));
        const auto constStats = computePlaneQualityStats(constant);
        require(constStats.valid, "constant stats invalid");
        require(std::abs(constStats.mean - 77.0) < 1e-6, "constant mean mismatch");
        require(constStats.stdDev < 1e-9, "constant std nonzero");
        require(constStats.informationEntropy < 1e-9, "constant IE nonzero");
        require(constStats.averageGradient < 1e-9, "constant AG nonzero");
        require(std::isfinite(constStats.piqe) && std::abs(constStats.piqe - 100.0) < 1e-6,
                "constant PIQE mismatch");
        require(constStats.minVal == 77 && constStats.maxVal == 77, "constant min/max mismatch");

        // 3. Exact 50/50 split image: IE == 1.0 bit/px.
        cv::Mat split(64, 64, CV_8UC1, cv::Scalar(20));
        split.rowRange(32, 64).setTo(200);
        const auto splitStats = computePlaneQualityStats(split);
        require(splitStats.valid, "split stats invalid");
        require(std::abs(splitStats.informationEntropy - 1.0) < 1e-6, "binary split IE != 1.0");
        require(splitStats.averageGradient > 0.0, "binary split AG <= 0");

        // 4. Pair PSNR / MAE / MaxDiff / Changed%: identical & constant shift.
        const cv::Mat sharp = buildNaturalLikeThermalTestImage(160, 192);
        const auto selfMetrics = computePairQualityMetrics(sharp, sharp);
        require(selfMetrics.valid, "self pair metrics invalid");
        require(std::isinf(selfMetrics.psnr) && selfMetrics.psnr > 0.0,
                "identical PSNR should be +inf");
        require(selfMetrics.mae == 0.0 && selfMetrics.maxAbsDiff == 0
                    && selfMetrics.changedPixelPercent == 0.0,
                "identical diff metrics nonzero");

        cv::Mat shifted;
        cv::add(sharp, cv::Scalar(10), shifted);
        const auto shiftMetrics = computePairQualityMetrics(
            cv::Mat(64, 64, CV_8UC1, cv::Scalar(50)),
            cv::Mat(64, 64, CV_8UC1, cv::Scalar(60))
        );
        const double expectedPsnr = 10.0 * std::log10((255.0 * 255.0) / 100.0);
        require(shiftMetrics.valid, "shift metrics invalid");
        require(std::abs(shiftMetrics.mae - 10.0) < 1e-6, "shift MAE mismatch");
        require(shiftMetrics.maxAbsDiff == 10, "shift maxAbsDiff mismatch");
        require(std::abs(shiftMetrics.changedPixelPercent - 100.0) < 1e-6,
                "shift changed% mismatch");
        require(std::abs(shiftMetrics.psnr - expectedPsnr) < 1e-5,
                "shift PSNR mismatch");

        // 5. Sharp vs Heavy Blur: sharp has higher AG and lower (better) PIQE.
        cv::Mat blurred;
        cv::GaussianBlur(sharp, blurred, cv::Size(15, 15), 4.0);
        const auto sharpStats = computePlaneQualityStats(sharp);
        const auto blurStats = computePlaneQualityStats(blurred);
        require(sharpStats.valid && blurStats.valid, "sharp/blur stats invalid");
        require(sharpStats.averageGradient > blurStats.averageGradient,
                "expected sharp AG > blurred AG");
        require(sharpStats.piqe > 0.0 && sharpStats.piqe <= 100.0, "sharp PIQE out of range");
        require(blurStats.piqe > 0.0 && blurStats.piqe <= 100.0, "blur PIQE out of range");
        if (!(blurStats.piqe > sharpStats.piqe)) {
            std::ostringstream oss;
            oss << "expected heavy blur to worsen (increase) PIQE (sharp="
                << sharpStats.piqe << ", blur=" << blurStats.piqe << ")";
            throw std::runtime_error(oss.str());
        }

        // 5b. Strong AWGN should also worsen PIQE vs sharp natural texture.
        cv::Mat awgnNoise(sharp.size(), CV_16SC1);
        cv::RNG noiseRng(999);
        noiseRng.fill(awgnNoise, cv::RNG::NORMAL, 0.0, 35.0);
        cv::Mat sharp16;
        sharp.convertTo(sharp16, CV_16SC1);
        cv::Mat noisy;
        cv::Mat noisy16 = sharp16 + awgnNoise;
        noisy16.convertTo(noisy, CV_8UC1);
        const auto noisyStats = computePlaneQualityStats(noisy);
        require(noisyStats.valid && noisyStats.piqe > 0.0 && noisyStats.piqe <= 100.0,
                "noisy PIQE out of range");
        if (!(noisyStats.piqe > sharpStats.piqe)) {
            std::ostringstream oss;
            oss << "expected strong AWGN to worsen (increase) PIQE (sharp="
                << sharpStats.piqe << ", noisy=" << noisyStats.piqe << ")";
            throw std::runtime_error(oss.str());
        }

        // 6. Tiny images (< 16x16) still produce valid bounded PIQE without crashing.
        const cv::Mat tiny(8, 12, CV_8UC1, cv::Scalar(30));
        const auto tinyStats = computePlaneQualityStats(tiny);
        require(tinyStats.valid && std::isfinite(tinyStats.piqe)
                    && tinyStats.piqe > 0.0 && tinyStats.piqe <= 100.0,
                "tiny image PIQE invalid");

        std::cout << "Thermal quality metrics contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
