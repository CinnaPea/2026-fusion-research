#include "core/enhancement/msgf_thermal_enhancer.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace qart::core::enhancement {
namespace {

cv::Mat clamp01(const cv::Mat& m)
{
    cv::Mat out;
    cv::min(cv::max(m, 0.0f), 1.0f, out);
    return out;
}

cv::Mat boxFilterUnnormalized(const cv::Mat& img, int r)
{
    const int k = 2 * r + 1;
    cv::Mat out;
    cv::boxFilter(
        img,
        out,
        -1,
        cv::Size(k, k),
        cv::Point(-1, -1),
        false,
        cv::BORDER_REFLECT101
    );
    return out;
}

cv::Mat guidedFilterGray(const cv::Mat& guide, const cv::Mat& src, int r, float eps)
{
    cv::Mat I;
    cv::Mat p;
    guide.convertTo(I, CV_32F);
    src.convertTo(p, CV_32F);

    const float area = static_cast<float>((2 * r + 1) * (2 * r + 1));

    const cv::Mat meanI = boxFilterUnnormalized(I, r) / area;
    const cv::Mat meanP = boxFilterUnnormalized(p, r) / area;
    const cv::Mat meanII = boxFilterUnnormalized(I.mul(I), r) / area;
    const cv::Mat meanIp = boxFilterUnnormalized(I.mul(p), r) / area;

    cv::Mat varI = meanII - meanI.mul(meanI);
    cv::max(varI, 0.0f, varI);
    const cv::Mat covIp = meanIp - meanI.mul(meanP);

    const cv::Mat a = covIp / (varI + eps);
    const cv::Mat b = meanP - a.mul(meanI);

    const cv::Mat meanA = boxFilterUnnormalized(a, r) / area;
    const cv::Mat meanB = boxFilterUnnormalized(b, r) / area;

    const cv::Mat q = meanA.mul(I) + meanB;
    return clamp01(q);
}

} // namespace

MsgfThermalEnhancer::MsgfThermalEnhancer(MsgfParameters params)
    : params_(std::move(params))
{
    if (params_.radii.empty()) {
        throw std::invalid_argument("MSGF requires nonempty scale radii");
    }
    for (int r : params_.radii) {
        if (r < 1) {
            throw std::invalid_argument("MSGF scale radius must be >= 1");
        }
    }
    if (params_.baseClaheTiles < 1) {
        throw std::invalid_argument("MSGF CLAHE tile size must be >= 1");
    }
}

MsgfEnhancementResult MsgfThermalEnhancer::enhance(const cv::Mat& plane) const
{
    if (plane.empty() || plane.dims != 2 || plane.type() != CV_8UC1) {
        throw std::invalid_argument("MSGF requires a nonempty 2D CV_8UC1 thermal plane");
    }

    cv::Mat t1;
    plane.convertTo(t1, CV_32F, 1.0 / 255.0);

    cv::Mat guide;
    cv::GaussianBlur(
        t1,
        guide,
        cv::Size(0, 0),
        params_.preSigma,
        params_.preSigma,
        cv::BORDER_DEFAULT
    );

    std::vector<cv::Mat> outs;
    outs.reserve(params_.radii.size());
    for (int r : params_.radii) {
        cv::Mat q = guidedFilterGray(guide, t1, r, params_.eps);
        outs.push_back(q);
        guide = q;
    }

    const cv::Mat& base = outs.back();
    std::vector<cv::Mat> details;
    details.reserve(outs.size());

    cv::Mat d0 = t1 - outs[0];
    cv::min(cv::max(d0, -1.0f), 1.0f, d0);
    details.push_back(d0);

    for (std::size_t i = 1; i < outs.size(); ++i) {
        cv::Mat d = outs[i - 1] - outs[i];
        cv::min(cv::max(d, -1.0f), 1.0f, d);
        details.push_back(d);
    }

    cv::Mat base8;
    clamp01(base).convertTo(base8, CV_8U, 255.0);

    auto clahe = cv::createCLAHE(
        params_.baseClaheClip,
        cv::Size(params_.baseClaheTiles, params_.baseClaheTiles)
    );
    cv::Mat beq8;
    clahe->apply(base8, beq8);

    cv::Mat mix;
    beq8.convertTo(mix, CV_32F, 1.0 / 255.0);

    std::vector<float> gains = params_.detailGains;
    if (gains.empty()) {
        gains = {0.15f, 0.10f};
        while (gains.size() < details.size()) {
            gains.push_back(0.05f);
        }
    }

    const std::size_t n = std::min(gains.size(), details.size());
    for (std::size_t i = 0; i < n; ++i) {
        mix += gains[i] * details[i];
    }
    mix = clamp01(mix);

    cv::Mat output;
    mix.convertTo(output, CV_8U, 255.0);
    return {std::move(output)};
}

} // namespace qart::core::enhancement
