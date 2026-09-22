//
// Created by hakgu on 8/11/2026.
//

#include "pair_validation_status.h"

#include <stdexcept>

namespace qart::core::domain::validation {

std::string_view toString(
    const PairValidationStatus sts
) noexcept
{
    switch (sts) {
        case PairValidationStatus::Valid:
            return "VALID";

        case PairValidationStatus::MissingVisible:
            return "MISSING_VISIBLE";

        case PairValidationStatus::MissingThermal:
            return "MISSING_THERMAL";

        case PairValidationStatus::UnsupportedVisibleFormat:
            return "UNSUPPORTED_VISIBLE_FORMAT";

        case PairValidationStatus::UnsupportedThermalFormat:
            return "UNSUPPORTED_THERMAL_FORMAT";

        case PairValidationStatus::VisibleDecodeFailed:
            return "VISIBLE_DECODE_FAILED";

        case PairValidationStatus::ThermalDecodeFailed:
            return "THERMAL_DECODE_FAILED";

        case PairValidationStatus::DimensionMismatch:
            return "DIMENSION_MISMATCH";

        case PairValidationStatus::UnexpectedVisibleDimensions:
            return "UNEXPECTED_VISIBLE_DIMENSIONS";

        case PairValidationStatus::UnexpectedThermalDimensions:
            return "UNEXPECTED_THERMAL_DIMENSIONS";

        case PairValidationStatus::DuplicateVisibleStem:
            return "DUPLICATE_VISIBLE_STEM";

        case PairValidationStatus::DuplicateThermalStem:
            return "DUPLICATE_THERMAL_STEM";
    }

    return "";
}

PairValidationStatus pairValidationStatusFromString(
    const std::string& value
)
{
    if (value == "VALID") {
        return PairValidationStatus::Valid;
    }

    if (value == "MISSING_VISIBLE") {
        return PairValidationStatus::MissingVisible;
    }

    if (value == "MISSING_THERMAL") {
        return PairValidationStatus::MissingThermal;
    }

    if (value == "UNSUPPORTED_VISIBLE_FORMAT") {
        return PairValidationStatus::UnsupportedVisibleFormat;
    }

    if (value == "UNSUPPORTED_THERMAL_FORMAT") {
        return PairValidationStatus::UnsupportedThermalFormat;
    }

    if (value == "VISIBLE_DECODE_FAILED") {
        return PairValidationStatus::VisibleDecodeFailed;
    }

    if (value == "THERMAL_DECODE_FAILED") {
        return PairValidationStatus::ThermalDecodeFailed;
    }

    if (value == "DIMENSION_MISMATCH") {
        return PairValidationStatus::DimensionMismatch;
    }

    if (value == "UNEXPECTED_VISIBLE_DIMENSIONS") {
        return PairValidationStatus::UnexpectedVisibleDimensions;
    }

    if (value == "UNEXPECTED_THERMAL_DIMENSIONS") {
        return PairValidationStatus::UnexpectedThermalDimensions;
    }

    if (value == "DUPLICATE_VISIBLE_STEM") {
        return PairValidationStatus::DuplicateVisibleStem;
    }

    if (value == "DUPLICATE_THERMAL_STEM") {
        return PairValidationStatus::DuplicateThermalStem;
    }

    throw std::invalid_argument(
        "Unknown pair validation status: '" + value + "'."
    );
}

} // namespace qart::core::domain::validation