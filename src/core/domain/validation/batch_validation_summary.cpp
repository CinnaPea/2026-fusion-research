//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/batch_validation_summary.h"

#include <array>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace qart::core::domain::validation {

namespace {

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

} // namespace

BatchValidationSummary::BatchValidationSummary(
    const int totalPairs,
    const int validPairs,
    const int invalidPairs,
    std::vector<ValidationStatusCount> statusCounts
)
    : totalPairs_(totalPairs),
      validPairs_(validPairs),
      invalidPairs_(invalidPairs),
      statusCounts_(std::move(statusCounts))
{
    validate();
}

int BatchValidationSummary::totalPairs() const noexcept
{
    return totalPairs_;
}

int BatchValidationSummary::validPairs() const noexcept
{
    return validPairs_;
}

int BatchValidationSummary::invalidPairs() const noexcept
{
    return invalidPairs_;
}

const std::vector<ValidationStatusCount>&
BatchValidationSummary::statusCounts() const noexcept
{
    return statusCounts_;
}

int BatchValidationSummary::countFor(
    const PairValidationStatus status
) const
{
    for (const ValidationStatusCount& statusCount : statusCounts_) {
        if (statusCount.status() == status) {
            return statusCount.count();
        }
    }

    throw std::invalid_argument(
        "Validation status is absent from the summary: "
        + std::string(toString(status))
    );
}

void BatchValidationSummary::validate() const
{
    if (totalPairs_ < 0) {
        throw std::invalid_argument(
            "Batch total_pairs must not be negative."
        );
    }

    if (validPairs_ < 0) {
        throw std::invalid_argument(
            "Batch valid_pairs must not be negative."
        );
    }

    if (invalidPairs_ < 0) {
        throw std::invalid_argument(
            "Batch invalid_pairs must not be negative."
        );
    }

    if (validPairs_ + invalidPairs_ != totalPairs_) {
        throw std::invalid_argument(
            "valid_pairs plus invalid_pairs must equal total_pairs."
        );
    }

    std::set<PairValidationStatus> observedStatuses;

    for (const ValidationStatusCount& statusCount : statusCounts_) {
        const auto insertion =
            observedStatuses.insert(
                statusCount.status()
            );

        if (!insertion.second) {
            throw std::invalid_argument(
                "Batch status_counts must not contain duplicate "
                "status '"
                + std::string(
                    toString(statusCount.status())
                )
                + "'."
            );
        }
    }

    const std::set<PairValidationStatus> expectedStatuses(
        allStatuses.begin(),
        allStatuses.end()
    );

    if (observedStatuses != expectedStatuses) {
        throw std::invalid_argument(
            "Batch status_counts must contain every validation "
            "status exactly once."
        );
    }

    long long countedPairs = 0;

    for (const ValidationStatusCount& statusCount : statusCounts_) {
        countedPairs += statusCount.count();
    }

    if (countedPairs != totalPairs_) {
        throw std::invalid_argument(
            "The sum of status counts must equal total_pairs."
        );
    }

    if (
        countFor(PairValidationStatus::Valid)
        != validPairs_
    ) {
        throw std::invalid_argument(
            "The VALID status count must equal valid_pairs."
        );
    }
}

bool operator==(
    const BatchValidationSummary& left,
    const BatchValidationSummary& right
) noexcept
{
    return left.totalPairs_ == right.totalPairs_
        && left.validPairs_ == right.validPairs_
        && left.invalidPairs_ == right.invalidPairs_
        && left.statusCounts_ == right.statusCounts_;
}

bool operator!=(
    const BatchValidationSummary& left,
    const BatchValidationSummary& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::domain::validation