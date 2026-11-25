#ifndef VISUAL_THERMAL_CONCEPT_MSGF_THERMAL_ENHANCER_H
#define VISUAL_THERMAL_CONCEPT_MSGF_THERMAL_ENHANCER_H

#pragma once

#include <opencv2/core.hpp>
#include <vector>

namespace qart::core::enhancement {

struct MsgfParameters {
    std::vector<int> radii{3, 8, 16, 32};
    float eps = 1e-3f;
    float preSigma = 1.2f;
    double baseClaheClip = 2.0;
    int baseClaheTiles = 8;
    std::vector<float> detailGains{0.18f, 0.12f, 0.06f, 0.03f};
};

struct MsgfEnhancementResult {
    cv::Mat outputImage;
};

// Experimental Multi-Scale Guided Filter (MSGF) baseline ported from
// ref_msgf::msgf_thermal (RGB_Thermal_Fusion__C__) on n5_branch for
// side-by-side comparison against N5 Post-Half.
class MsgfThermalEnhancer final {
public:
    explicit MsgfThermalEnhancer(MsgfParameters params = MsgfParameters{});

    [[nodiscard]]
    const MsgfParameters& parameters() const noexcept { return params_; }

    [[nodiscard]]
    MsgfEnhancementResult enhance(const cv::Mat& plane) const;

private:
    MsgfParameters params_;
};

} // namespace qart::core::enhancement

#endif // VISUAL_THERMAL_CONCEPT_MSGF_THERMAL_ENHANCER_H
