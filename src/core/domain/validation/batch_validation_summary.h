//
// Created by hakgu on 8/16/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_SUMMARY_H
#define VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_SUMMARY_H

#pragma once

#include "core/domain/validation/pair_validation_status.h"
#include "core/domain/validation/validation_status_count.h"

#include <vector>

namespace qart::core::domain::validation {

    class BatchValidationSummary final
    {
    public:
        BatchValidationSummary(
            int totalPairs,
            int validPairs,
            int invalidPairs,
            std::vector<ValidationStatusCount> statusCounts
        );

        [[nodiscard]]
        int totalPairs() const noexcept;

        [[nodiscard]]
        int validPairs() const noexcept;

        [[nodiscard]]
        int invalidPairs() const noexcept;

        [[nodiscard]]
        const std::vector<ValidationStatusCount>&
        statusCounts() const noexcept;

        [[nodiscard]]
        int countFor(
            PairValidationStatus status
        ) const;

        friend bool operator==(
            const BatchValidationSummary& left,
            const BatchValidationSummary& right
        ) noexcept;

        friend bool operator!=(
            const BatchValidationSummary& left,
            const BatchValidationSummary& right
        ) noexcept;

    private:
        void validate() const;

        int totalPairs_;
        int validPairs_;
        int invalidPairs_;
        std::vector<ValidationStatusCount> statusCounts_;
    };

} // namespace qart::core::domain::validation

#endif //VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_SUMMARY_H