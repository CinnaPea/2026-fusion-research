//
// Created by hakgu on 8/20/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_PREVIEW_PAIR_SELECTOR_H
#define VISUAL_THERMAL_CONCEPT_PREVIEW_PAIR_SELECTOR_H

#pragma once

#include "core/domain/validation/pair_validation_result.h"
#include "core/visualization/preview_selection.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace qart::core::visualization {

    class PreviewSelectionError final
        : public std::invalid_argument
    {
    public:
        using std::invalid_argument::invalid_argument;
    };


    class PreviewPairSelector final
    {
    public:
        [[nodiscard]]
        PreviewSelection select(
            const std::vector<
                domain::validation::PairValidationResult
            >& results,
            const PreviewSelectionRequest& request
        ) const;

    private:
        struct PartitionGroup
        {
            std::string partitionId;

            std::vector<
                const domain::validation::
                    PairValidationResult*
            > validResults;
        };

        [[nodiscard]]
        static std::vector<std::size_t>
        evenlySpacedIndices(
            std::size_t availableCount,
            int requestedCount
        );

        [[nodiscard]]
        static std::vector<std::string>
        deduplicatePairIds(
            const std::vector<std::string>& pairIds
        );

        [[nodiscard]]
        static std::vector<PartitionGroup>
        groupValidResultsByPartition(
            const std::vector<
                domain::validation::PairValidationResult
            >& results
        );

        [[nodiscard]]
        static std::vector<
            domain::validation::PairValidationResult
        > selectAutomaticResults(
            const std::vector<
                domain::validation::PairValidationResult
            >& results,
            int countPerPartition
        );
    };

} // namespace qart::core::visualization

#endif //VISUAL_THERMAL_CONCEPT_PREVIEW_PAIR_SELECTOR_H