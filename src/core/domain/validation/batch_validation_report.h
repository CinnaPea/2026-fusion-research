//
// Created by hakgu on 8/16/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_REPORT_H
#define VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_REPORT_H

#pragma once

#include "core/domain/validation/batch_validation_summary.h"
#include "core/domain/validation/pair_validation_result.h"

#include <vector>

namespace qart::core::domain::validation {

    class BatchValidationReport final
    {
    public:
        BatchValidationReport(
            std::vector<PairValidationResult> results,
            BatchValidationSummary summary
        );

        [[nodiscard]]
        const std::vector<PairValidationResult>&
        results() const noexcept;

        [[nodiscard]]
        const BatchValidationSummary&
        summary() const noexcept;

        friend bool operator==(
            const BatchValidationReport& left,
            const BatchValidationReport& right
        ) noexcept;

        friend bool operator!=(
            const BatchValidationReport& left,
            const BatchValidationReport& right
        ) noexcept;

    private:
        void validate() const;

        std::vector<PairValidationResult> results_;
        BatchValidationSummary summary_;
    };

} // namespace qart::core::domain::validation

#endif //VISUAL_THERMAL_CONCEPT_BATCH_VALIDATION_REPORT_H