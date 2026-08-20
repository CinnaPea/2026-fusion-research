//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/validation_status_count.h"

#include <stdexcept>

namespace {

    using qart::core::domain::validation::PairValidationStatus;
    using qart::core::domain::validation::ValidationStatusCount;

    bool rejectsNegativeCount()
    {
        try {
            const ValidationStatusCount count(
                PairValidationStatus::Valid,
                -1
            );

            static_cast<void>(count);
        }
        catch (const std::invalid_argument&) {
            return true;
        }

        return false;
    }

} // namespace

int main()
{
    const ValidationStatusCount zero(
        PairValidationStatus::Valid,
        0
    );

    if (zero.status() != PairValidationStatus::Valid) {
        return 1;
    }

    if (zero.count() != 0) {
        return 2;
    }

    const ValidationStatusCount positive(
        PairValidationStatus::MissingVisible,
        7
    );

    if (
        positive.status()
        != PairValidationStatus::MissingVisible
    ) {
        return 3;
    }

    if (positive.count() != 7) {
        return 4;
    }

    const ValidationStatusCount equivalent(
        PairValidationStatus::MissingVisible,
        7
    );

    if (positive != equivalent) {
        return 5;
    }

    const ValidationStatusCount differentStatus(
        PairValidationStatus::MissingThermal,
        7
    );

    if (positive == differentStatus) {
        return 6;
    }

    const ValidationStatusCount differentCount(
        PairValidationStatus::MissingVisible,
        8
    );

    if (positive == differentCount) {
        return 7;
    }

    if (!rejectsNegativeCount()) {
        return 8;
    }

    return 0;
}