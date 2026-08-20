//
// Created by hakgu on 8/16/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_PROGRESS_H
#define VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_PROGRESS_H

#pragma once

#include "core/domain/validation/pair_validation_result.h"

namespace qart::core::domain::validation {

    class BatchValidationProgress final
    {
    public:
        BatchValidationProgress(
            int completedPairs,
            int totalPairs,
            PairValidationResult currentResult,
            double pairElapsedSeconds,
            double batchElapsedSeconds
        );

        [[nodiscard]]
        int completedPairs() const noexcept;

        [[nodiscard]]
        int totalPairs() const noexcept;

        [[nodiscard]]
        const PairValidationResult&
        currentResult() const noexcept;

        [[nodiscard]]
        double pairElapsedSeconds() const noexcept;

        [[nodiscard]]
        double batchElapsedSeconds() const noexcept;

        [[nodiscard]]
        int remainingPairs() const noexcept;

        [[nodiscard]]
        double averageSecondsPerPair() const noexcept;

        [[nodiscard]]
        double estimatedRemainingSeconds() const noexcept;

        friend bool operator==(
            const BatchValidationProgress& left,
            const BatchValidationProgress& right
        ) noexcept;

        friend bool operator!=(
            const BatchValidationProgress& left,
            const BatchValidationProgress& right
        ) noexcept;

    private:
        void validate() const;

        int completedPairs_;
        int totalPairs_;

        PairValidationResult currentResult_;

        double pairElapsedSeconds_;
        double batchElapsedSeconds_;
    };

} // namespace qart::core::domain::validation

#endif //VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_PROGRESS_H