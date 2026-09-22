//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/batch_validation_report.h"

#include "core/domain/validation/pair_validation_status.h"

#include <array>
#include <map>
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

BatchValidationReport::BatchValidationReport(
    std::vector<PairValidationResult> results,
    BatchValidationSummary summary
)
    : results_(std::move(results)),
      summary_(std::move(summary))
{
    validate();
}

const std::vector<PairValidationResult>&
BatchValidationReport::results() const noexcept
{
    return results_;
}

const BatchValidationSummary&
BatchValidationReport::summary() const noexcept
{
    return summary_;
}

void BatchValidationReport::validate() const
{
    if (
        results_.size()
        != static_cast<std::size_t>(
            summary_.totalPairs()
        )
    ) {
        throw std::invalid_argument(
            "Batch result count must equal summary total_pairs."
        );
    }

    std::set<std::string> pairIdentifiers;

    for (const PairValidationResult& result : results_) {
        const auto insertion =
            pairIdentifiers.insert(
                result.pair().pairId()
            );

        if (!insertion.second) {
            throw std::invalid_argument(
                "Batch validation results must contain "
                "unique pair IDs."
            );
        }
    }

    std::map<PairValidationStatus, int>
        observedCounts;

    for (const PairValidationStatus status : allStatuses) {
        observedCounts.emplace(
            status,
            0
        );
    }

    for (const PairValidationResult& result : results_) {
        ++observedCounts[result.status()];
    }

    for (const PairValidationStatus status : allStatuses) {
        if (
            observedCounts.at(status)
            != summary_.countFor(status)
        ) {
            throw std::invalid_argument(
                "Batch summary does not match individual "
                "result count for status '"
                + std::string(toString(status))
                + "'."
            );
        }
    }
}

bool operator==(
    const BatchValidationReport& left,
    const BatchValidationReport& right
) noexcept
{
    return left.results_ == right.results_
        && left.summary_ == right.summary_;
}

bool operator!=(
    const BatchValidationReport& left,
    const BatchValidationReport& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::domain::validation