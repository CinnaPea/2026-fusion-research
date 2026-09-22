//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/batch_validation_progress.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace qart::core::domain::validation {

BatchValidationProgress::BatchValidationProgress(
    const int completedPairs,
    const int totalPairs,
    PairValidationResult currentResult,
    const double pairElapsedSeconds,
    const double batchElapsedSeconds
)
    : completedPairs_(completedPairs),
      totalPairs_(totalPairs),
      currentResult_(std::move(currentResult)),
      pairElapsedSeconds_(pairElapsedSeconds),
      batchElapsedSeconds_(batchElapsedSeconds)
{
    validate();
}

int BatchValidationProgress::completedPairs() const noexcept
{
    return completedPairs_;
}

int BatchValidationProgress::totalPairs() const noexcept
{
    return totalPairs_;
}

const PairValidationResult&
BatchValidationProgress::currentResult() const noexcept
{
    return currentResult_;
}

double
BatchValidationProgress::pairElapsedSeconds() const noexcept
{
    return pairElapsedSeconds_;
}

double
BatchValidationProgress::batchElapsedSeconds() const noexcept
{
    return batchElapsedSeconds_;
}

int BatchValidationProgress::remainingPairs() const noexcept
{
    return totalPairs_ - completedPairs_;
}

double
BatchValidationProgress::averageSecondsPerPair() const noexcept
{
    return batchElapsedSeconds_
        / static_cast<double>(completedPairs_);
}

double
BatchValidationProgress::estimatedRemainingSeconds() const noexcept
{
    return averageSecondsPerPair()
        * static_cast<double>(remainingPairs());
}

void BatchValidationProgress::validate() const
{
    if (totalPairs_ <= 0) {
        throw std::invalid_argument(
            "Progress total_pairs must be positive."
        );
    }

    if (
        completedPairs_ < 1
        || completedPairs_ > totalPairs_
    ) {
        throw std::invalid_argument(
            "Progress completed_pairs must be between one "
            "and total_pairs."
        );
    }

    if (
        !std::isfinite(pairElapsedSeconds_)
        || !std::isfinite(batchElapsedSeconds_)
    ) {
        throw std::invalid_argument(
            "Progress durations must be finite."
        );
    }

    if (
        pairElapsedSeconds_ < 0.0
        || batchElapsedSeconds_ < 0.0
    ) {
        throw std::invalid_argument(
            "Progress durations must not be negative."
        );
    }
}

bool operator==(
    const BatchValidationProgress& left,
    const BatchValidationProgress& right
) noexcept
{
    return left.completedPairs_ == right.completedPairs_
        && left.totalPairs_ == right.totalPairs_
        && left.currentResult_ == right.currentResult_
        && left.pairElapsedSeconds_
            == right.pairElapsedSeconds_
        && left.batchElapsedSeconds_
            == right.batchElapsedSeconds_;
}

bool operator!=(
    const BatchValidationProgress& left,
    const BatchValidationProgress& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::domain::validation