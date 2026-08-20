//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/batch_validation_summary.h"

#include "core/domain/validation/pair_validation_status.h"
#include "core/domain/validation/validation_status_count.h"

#include <array>
#include <stdexcept>
#include <vector>

namespace {

using qart::core::domain::validation::BatchValidationSummary;
using qart::core::domain::validation::PairValidationStatus;
using qart::core::domain::validation::ValidationStatusCount;

constexpr std::array<PairValidationStatus, 12>
allStatuses = {
    PairValidationStatus::Valid,

    PairValidationStatus::MissingVisible,
    PairValidationStatus::MissingThermal,

    PairValidationStatus::UnsupportedVisibleFormat,
    PairValidationStatus::UnsupportedThermalFormat,

    PairValidationStatus::VisibleDecodeFailed,
    PairValidationStatus::ThermalDecodeFailed,

    PairValidationStatus::DimensionMismatch,
    PairValidationStatus::UnexpectedVisibleDimensions,
    PairValidationStatus::UnexpectedThermalDimensions,

    PairValidationStatus::DuplicateVisibleStem,
    PairValidationStatus::DuplicateThermalStem
};

std::vector<ValidationStatusCount> zeroCounts()
{
    std::vector<ValidationStatusCount> counts;

    for (const PairValidationStatus status : allStatuses) {
        counts.emplace_back(
            status,
            0
        );
    }

    return counts;
}

std::vector<ValidationStatusCount> countsForMixedBatch()
{
    std::vector<ValidationStatusCount> counts;

    for (const PairValidationStatus status : allStatuses) {
        int count = 0;

        if (status == PairValidationStatus::Valid) {
            count = 2;
        }
        else if (
            status
            == PairValidationStatus::MissingVisible
        ) {
            count = 1;
        }
        else if (
            status
            == PairValidationStatus::ThermalDecodeFailed
        ) {
            count = 1;
        }

        counts.emplace_back(
            status,
            count
        );
    }

    return counts;
}

bool rejectsNegativeTotal()
{
    try {
        const BatchValidationSummary summary(
            -1,
            0,
            0,
            zeroCounts()
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsNegativeValid()
{
    try {
        const BatchValidationSummary summary(
            0,
            -1,
            1,
            zeroCounts()
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsNegativeInvalid()
{
    try {
        const BatchValidationSummary summary(
            0,
            1,
            -1,
            zeroCounts()
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsArithmeticMismatch()
{
    try {
        auto counts = zeroCounts();

        for (ValidationStatusCount& count : counts) {
            if (count.status() == PairValidationStatus::Valid) {
                count = ValidationStatusCount(
                    PairValidationStatus::Valid,
                    1
                );

                break;
            }
        }

        const BatchValidationSummary summary(
            2,
            1,
            0,
            counts
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsDuplicateStatus()
{
    try {
        auto counts = zeroCounts();

        counts.emplace_back(
            PairValidationStatus::Valid,
            0
        );

        const BatchValidationSummary summary(
            0,
            0,
            0,
            counts
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsMissingStatus()
{
    try {
        auto counts = zeroCounts();

        for (auto iterator = counts.begin();
             iterator != counts.end();
             ++iterator) {
            if (
                iterator->status()
                == PairValidationStatus::ThermalDecodeFailed
            ) {
                counts.erase(iterator);
                break;
            }
        }

        const BatchValidationSummary summary(
            0,
            0,
            0,
            counts
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsCountSumMismatch()
{
    try {
        const BatchValidationSummary summary(
            1,
            0,
            1,
            zeroCounts()
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsValidCountMismatch()
{
    try {
        auto counts = zeroCounts();

        for (ValidationStatusCount& count : counts) {
            if (
                count.status()
                == PairValidationStatus::MissingVisible
            ) {
                count = ValidationStatusCount(
                    PairValidationStatus::MissingVisible,
                    1
                );

                break;
            }
        }

        const BatchValidationSummary summary(
            1,
            1,
            0,
            counts
        );

        static_cast<void>(summary);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const BatchValidationSummary empty(
        0,
        0,
        0,
        zeroCounts()
    );

    if (empty.totalPairs() != 0) {
        return 1;
    }

    if (empty.validPairs() != 0) {
        return 2;
    }

    if (empty.invalidPairs() != 0) {
        return 3;
    }

    if (
        empty.statusCounts().size()
        != allStatuses.size()
    ) {
        return 4;
    }

    for (const PairValidationStatus status : allStatuses) {
        if (empty.countFor(status) != 0) {
            return 5;
        }
    }

    const BatchValidationSummary mixed(
        4,
        2,
        2,
        countsForMixedBatch()
    );

    if (
        mixed.countFor(PairValidationStatus::Valid)
        != 2
    ) {
        return 6;
    }

    if (
        mixed.countFor(
            PairValidationStatus::MissingVisible
        )
        != 1
    ) {
        return 7;
    }

    if (
        mixed.countFor(
            PairValidationStatus::ThermalDecodeFailed
        )
        != 1
    ) {
        return 8;
    }

    const BatchValidationSummary equivalent(
        4,
        2,
        2,
        countsForMixedBatch()
    );

    if (mixed != equivalent) {
        return 9;
    }

    if (!rejectsNegativeTotal()) {
        return 10;
    }

    if (!rejectsNegativeValid()) {
        return 11;
    }

    if (!rejectsNegativeInvalid()) {
        return 12;
    }

    if (!rejectsArithmeticMismatch()) {
        return 13;
    }

    if (!rejectsDuplicateStatus()) {
        return 14;
    }

    if (!rejectsMissingStatus()) {
        return 15;
    }

    if (!rejectsCountSumMismatch()) {
        return 16;
    }

    if (!rejectsValidCountMismatch()) {
        return 17;
    }

    return 0;
}