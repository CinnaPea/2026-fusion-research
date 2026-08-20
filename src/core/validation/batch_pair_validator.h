//
// Created by hakgu on 8/17/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_BATCH_PAIR_VALIDATOR_H
#define VISUAL_THERMAL_CONCEPT_BATCH_PAIR_VALIDATOR_H

#pragma once

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/batch_validation_progress.h"
#include "core/domain/validation/batch_validation_report.h"
#include "core/domain/validation/batch_validation_summary.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/validation/pair_validator.h"

#include <filesystem>
#include <functional>
#include <vector>

namespace qart::core::validation {

    class BatchPairValidator final
    {
    public:
        using Clock = std::function<double()>;

        using ProgressObserver =
            std::function<void(
                const domain::validation::BatchValidationProgress&
            )>;

        explicit BatchPairValidator(
            PairValidator& pairValidator,
            Clock clock = {}
        );

        [[nodiscard]]
        domain::validation::BatchValidationReport validatePairs(
            const std::filesystem::path& datasetRoot,
            const std::vector<domain::datasets::DatasetPair>& pairs,
            const ProgressObserver& progressObserver = {}
        );

    private:
        static double defaultClock() noexcept;

        static void rejectDuplicatePairIds(
            const std::vector<domain::datasets::DatasetPair>& pairs
        );

        [[nodiscard]]
        static domain::validation::BatchValidationSummary buildSummary(
            const std::vector<
                domain::validation::PairValidationResult
            >& results
        );

        PairValidator& pairValidator_;
        Clock clock_;
    };

} // namespace qart::core::validation

#endif //VISUAL_THERMAL_CONCEPT_BATCH_PAIR_VALIDATOR_H