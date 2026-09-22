//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include <string>
#include <string_view>

#ifndef VISUAL_THERMAL_CONCEPT_IMAGE_DECODE_STATUS_H
#define VISUAL_THERMAL_CONCEPT_IMAGE_DECODE_STATUS_H

namespace qart::core::domain::imaging {
    enum class ImageDecodeStatus {
        Decoded,
        DecodeFailed
    };

    [[nodiscard]]
    std::string_view toString(ImageDecodeStatus sts) noexcept;

    [[nodiscard]]
    ImageDecodeStatus imageDecodeStatusFromString(const std::string& val);
}

#endif //VISUAL_THERMAL_CONCEPT_IMAGE_DECODE_STATUS_H