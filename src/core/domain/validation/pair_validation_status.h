//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include <string>
#include <string_view>

#ifndef VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_STATUS_H
#define VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_STATUS_H

namespace qart::core::domain::validation {
    enum class PairValidationStatus {
        Valid,
        MissingVisible,
        MissingThermal,
        UnsupportedVisibleFormat,
        UnsupportedThermalFormat,
        VisibleDecodeFailed,
        ThermalDecodeFailed,
        DimensionMismatch,
        UnexpectedVisibleDimensions,
        UnexpectedThermalDimensions,
        DuplicateVisibleStem,
        DuplicateThermalStem
    };

    [[nodiscard]]
    std::string_view toString(PairValidationStatus sts) noexcept;

    [[nodiscard]]
    PairValidationStatus pairValidationStatusFromString(const std::string& val);
}

#endif //VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_STATUS_H