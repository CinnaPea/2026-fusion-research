//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/validation/pair_validation_status.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::core::domain::validation::PairValidationStatus;
using qart::core::domain::validation::pairValidationStatusFromString;
using qart::core::domain::validation::toString;

struct StatusCase
{
    PairValidationStatus status;
    std::string text;
};

bool rejectsInvalidStatus(
    const std::string& value
)
{
    try {
        const PairValidationStatus status =
            pairValidationStatusFromString(value);

        static_cast<void>(status);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const std::vector<StatusCase> validCases = {
        {
            PairValidationStatus::Valid,
            "VALID"
        },
        {
            PairValidationStatus::MissingVisible,
            "MISSING_VISIBLE"
        },
        {
            PairValidationStatus::MissingThermal,
            "MISSING_THERMAL"
        },
        {
            PairValidationStatus::UnsupportedVisibleFormat,
            "UNSUPPORTED_VISIBLE_FORMAT"
        },
        {
            PairValidationStatus::UnsupportedThermalFormat,
            "UNSUPPORTED_THERMAL_FORMAT"
        },
        {
            PairValidationStatus::VisibleDecodeFailed,
            "VISIBLE_DECODE_FAILED"
        },
        {
            PairValidationStatus::ThermalDecodeFailed,
            "THERMAL_DECODE_FAILED"
        },
        {
            PairValidationStatus::DimensionMismatch,
            "DIMENSION_MISMATCH"
        },
        {
            PairValidationStatus::UnexpectedVisibleDimensions,
            "UNEXPECTED_VISIBLE_DIMENSIONS"
        },
        {
            PairValidationStatus::UnexpectedThermalDimensions,
            "UNEXPECTED_THERMAL_DIMENSIONS"
        },
        {
            PairValidationStatus::DuplicateVisibleStem,
            "DUPLICATE_VISIBLE_STEM"
        },
        {
            PairValidationStatus::DuplicateThermalStem,
            "DUPLICATE_THERMAL_STEM"
        }
    };

    for (const StatusCase& testCase : validCases) {
        if (toString(testCase.status) != testCase.text) {
            return 1;
        }

        if (
            pairValidationStatusFromString(testCase.text)
            != testCase.status
        ) {
            return 2;
        }
    }

    const std::vector<std::string> invalidValues = {
        "",
        "valid",
        "Valid",
        " VALID",
        "VALID ",
        "MISSING-VISIBLE",
        "DECODE_FAILED",
        "UNKNOWN"
    };

    for (const std::string& invalidValue : invalidValues) {
        if (!rejectsInvalidStatus(invalidValue)) {
            return 3;
        }
    }

    return 0;
}