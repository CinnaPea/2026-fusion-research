#include "core/enhancement/thermal_quality_metrics.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace qart::core::enhancement {
namespace {

double computeShannonEntropy(const cv::Mat& gray8)
{
    std::array<std::size_t, 256> hist{};
    for (int r = 0; r < gray8.rows; ++r) {
        const auto* rowPtr = gray8.ptr<unsigned char>(r);
        for (int c = 0; c < gray8.cols; ++c) {
            hist[rowPtr[c]]++;
        }
    }

    const double totalPx = static_cast<double>(gray8.total());
    if (totalPx <= 0.0) {
        return 0.0;
    }

    double entropy = 0.0;
    for (std::size_t count : hist) {
        if (count > 0) {
            const double p = static_cast<double>(count) / totalPx;
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

// Canonical Average Gradient (AG):
// (1 / ((M-1)(N-1))) * sum sqrt(((I(i+1,j)-I(i,j))^2 + (I(i,j+1)-I(i,j))^2) / 2)
double computeAverageGradient(const cv::Mat& gray8)
{
    if (gray8.rows < 2 || gray8.cols < 2) {
        return 0.0;
    }

    double sum = 0.0;
    for (int r = 0; r < gray8.rows - 1; ++r) {
        const auto* row = gray8.ptr<unsigned char>(r);
        const auto* nextRow = gray8.ptr<unsigned char>(r + 1);
        for (int c = 0; c < gray8.cols - 1; ++c) {
            const double cur = static_cast<double>(row[c]);
            const double dy = static_cast<double>(nextRow[c]) - cur;
            const double dx = static_cast<double>(row[c + 1]) - cur;
            sum += std::sqrt(0.5 * (dx * dx + dy * dy));
        }
    }

    const double denom =
        static_cast<double>(gray8.rows - 1) * static_cast<double>(gray8.cols - 1);
    return denom > 0.0 ? (sum / denom) : 0.0;
}

double variance1D(const double* data, int stride, int len)
{
    if (len <= 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (int i = 0; i < len; ++i) {
        sum += data[i * stride];
    }
    const double mean = sum / static_cast<double>(len);
    double sqSum = 0.0;
    for (int i = 0; i < len; ++i) {
        const double d = data[i * stride] - mean;
        sqSum += d * d;
    }
    return sqSum / static_cast<double>(len);
}

// Perception-based Image Quality Evaluator (PIQE, Venkatanath et al., 2015).
// Lower score indicates better perceptual quality (range (0, 100]).
double computePiqe(const cv::Mat& gray8)
{
    constexpr int kBlockSize = 16;
    constexpr int kSegLen = 6;
    constexpr double kActivityThreshold = 0.1;
    constexpr double kStructVarThreshold = 0.18;
    constexpr double kNoiseVarThreshold = 0.45;
    constexpr double kPoolingConst = 1.0;

    if (gray8.rows < kBlockSize || gray8.cols < kBlockSize) {
        return 100.0;
    }

    cv::Mat gray64;
    gray8.convertTo(gray64, CV_64F);

    cv::Mat mu;
    cv::GaussianBlur(
        gray64,
        mu,
        cv::Size(7, 7),
        7.0 / 6.0,
        7.0 / 6.0,
        cv::BORDER_REPLICATE
    );

    cv::Mat graySq = gray64.mul(gray64);
    cv::Mat meanSq;
    cv::GaussianBlur(
        graySq,
        meanSq,
        cv::Size(7, 7),
        7.0 / 6.0,
        7.0 / 6.0,
        cv::BORDER_REPLICATE
    );

    cv::Mat varMap = meanSq - mu.mul(mu);
    cv::max(varMap, 0.0, varMap);
    cv::Mat sigma;
    cv::sqrt(varMap, sigma);

    cv::Mat mscn = (gray64 - mu) / (sigma + 1.0);

    const int blockRows = mscn.rows / kBlockSize;
    const int blockCols = mscn.cols / kBlockSize;
    const int mscnStep = static_cast<int>(mscn.step / sizeof(double));

    int activeBlocks = 0;
    double distortionSum = 0.0;

    for (int br = 0; br < blockRows; ++br) {
        for (int bc = 0; bc < blockCols; ++bc) {
            const cv::Rect blockRect(
                bc * kBlockSize,
                br * kBlockSize,
                kBlockSize,
                kBlockSize
            );
            const cv::Mat block = mscn(blockRect);

            cv::Scalar blockMeanS;
            cv::Scalar blockStdS;
            cv::meanStdDev(block, blockMeanS, blockStdS);
            const double blockStd = blockStdS[0];
            const double blockVar = blockStd * blockStd;

            if (blockVar <= kActivityThreshold) {
                continue;
            }
            activeBlocks++;

            const double* blockPtr = block.ptr<double>(0);

            // 44 boundary segments (11 per edge, each of 6 consecutive MSCN samples)
            int lowVarSegCount = 0;
            const int segStarts = kBlockSize - kSegLen + 1; // 11
            for (int s = 0; s < segStarts; ++s) {
                // Top border (row 0)
                if (variance1D(blockPtr + s, 1, kSegLen) < kActivityThreshold) {
                    lowVarSegCount++;
                }
                // Bottom border (row 15)
                if (variance1D(blockPtr + 15 * mscnStep + s, 1, kSegLen) < kActivityThreshold) {
                    lowVarSegCount++;
                }
                // Left border (col 0)
                if (variance1D(blockPtr + s * mscnStep, mscnStep, kSegLen) < kActivityThreshold) {
                    lowVarSegCount++;
                }
                // Right border (col 15)
                if (variance1D(blockPtr + s * mscnStep + 15, mscnStep, kSegLen) < kActivityThreshold) {
                    lowVarSegCount++;
                }
            }

            // Middle columns (8th and 9th -> 0-indexed 7 and 8)
            const double stdCol7 = std::sqrt(std::max(0.0, variance1D(blockPtr + 7, mscnStep, kBlockSize)));
            const double stdCol8 = std::sqrt(std::max(0.0, variance1D(blockPtr + 8, mscnStep, kBlockSize)));
            const double centerColRatio =
                (blockStd > 1e-12) ? (0.5 * (stdCol7 + stdCol8) / blockStd) : 0.0;

            // Interior 10x10 region variance ratio for uncorrelated noise check
            const cv::Mat interior = block(cv::Rect(3, 3, 10, 10));
            cv::Scalar intMeanS;
            cv::Scalar intStdS;
            cv::meanStdDev(interior, intMeanS, intStdS);
            const double interiorVar = intStdS[0] * intStdS[0];
            const double interiorVarRatio =
                (blockVar > 1e-12) ? (interiorVar / blockVar) : 0.0;

            double dStruct = 0.0;
            if (blockVar < kStructVarThreshold
                || lowVarSegCount >= 18
                || (centerColRatio > 1.25 && lowVarSegCount >= 4)) {
                dStruct = std::clamp(1.0 - blockStd, 0.0, 1.0);
            }

            double dNoise = 0.0;
            if (blockVar > kNoiseVarThreshold
                && interiorVarRatio > 0.78
                && lowVarSegCount <= 2) {
                dNoise = std::clamp(blockStd, 0.0, 1.0);
            }

            distortionSum += std::max(dStruct, dNoise);
        }
    }

    if (activeBlocks == 0) {
        return 100.0;
    }

    const double rawScore =
        ((distortionSum + kPoolingConst)
         / (static_cast<double>(activeBlocks) + kPoolingConst))
        * 100.0;
    return std::clamp(rawScore, 0.0, 100.0);
}

} // namespace

PlaneQualityStats computePlaneQualityStats(const cv::Mat& gray8)
{
    PlaneQualityStats stats;
    if (gray8.empty() || gray8.dims != 2 || gray8.type() != CV_8UC1) {
        return stats;
    }

    stats.valid = true;

    cv::Scalar meanScalar;
    cv::Scalar stdScalar;
    cv::meanStdDev(gray8, meanScalar, stdScalar);
    stats.mean = meanScalar[0];
    stats.stdDev = stdScalar[0];

    double minV = 0.0;
    double maxV = 0.0;
    cv::minMaxLoc(gray8, &minV, &maxV);
    stats.minVal = static_cast<int>(std::round(minV));
    stats.maxVal = static_cast<int>(std::round(maxV));

    stats.informationEntropy = computeShannonEntropy(gray8);
    stats.averageGradient = computeAverageGradient(gray8);
    stats.piqe = computePiqe(gray8);

    return stats;
}

PairQualityMetrics computePairQualityMetrics(
    const cv::Mat& inputGray8,
    const cv::Mat& outputGray8)
{
    PairQualityMetrics metrics;
    if (inputGray8.empty()
        || outputGray8.empty()
        || inputGray8.dims != 2
        || outputGray8.dims != 2
        || inputGray8.type() != CV_8UC1
        || outputGray8.type() != CV_8UC1
        || inputGray8.size() != outputGray8.size()) {
        return metrics;
    }

    metrics.input = computePlaneQualityStats(inputGray8);
    metrics.output = computePlaneQualityStats(outputGray8);
    if (!metrics.input.valid || !metrics.output.valid) {
        return metrics;
    }

    cv::Mat absDiff;
    cv::absdiff(outputGray8, inputGray8, absDiff);

    double maxDiffD = 0.0;
    cv::minMaxLoc(absDiff, nullptr, &maxDiffD);
    metrics.maxAbsDiff = static_cast<int>(std::round(maxDiffD));
    metrics.mae = cv::mean(absDiff)[0];

    const double totalPx = static_cast<double>(absDiff.total());
    metrics.changedPixelPercent = (totalPx > 0.0)
        ? (100.0 * static_cast<double>(cv::countNonZero(absDiff)) / totalPx)
        : 0.0;

    const double l2Sq = cv::norm(outputGray8, inputGray8, cv::NORM_L2SQR);
    const double mse = totalPx > 0.0 ? (l2Sq / totalPx) : 0.0;
    metrics.psnr = (mse <= 1e-12)
        ? std::numeric_limits<double>::infinity()
        : (10.0 * std::log10((255.0 * 255.0) / mse));

    metrics.valid = true;
    return metrics;
}

} // namespace qart::core::enhancement
