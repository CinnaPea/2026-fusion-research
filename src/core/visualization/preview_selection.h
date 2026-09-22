//
// Created by hakgu on 8/20/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_PREVIEW_SELECTION_H
#define VISUAL_THERMAL_CONCEPT_PREVIEW_SELECTION_H

#pragma once

#include "core/domain/validation/pair_validation_result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace qart::core::visualization {

    class PreviewSelectionRequest final
    {
    public:
        explicit PreviewSelectionRequest(
            int automaticCountPerPartition = 5,
            std::vector<std::string> explicitPairIds = {}
        );

        [[nodiscard]]
        int automaticCountPerPartition() const noexcept;

        [[nodiscard]]
        const std::vector<std::string>&
        explicitPairIds() const noexcept;

    private:
        void validate() const;

        int automaticCountPerPartition_;
        std::vector<std::string> explicitPairIds_;
    };


    class PreviewSelection final
    {
    public:
        PreviewSelection(
            std::vector<
                domain::validation::PairValidationResult
            > automaticResults,
            std::vector<
                domain::validation::PairValidationResult
            > explicitResults
        );

        [[nodiscard]]
        const std::vector<
            domain::validation::PairValidationResult
        >& automaticResults() const noexcept;

        [[nodiscard]]
        const std::vector<
            domain::validation::PairValidationResult
        >& explicitResults() const noexcept;

        [[nodiscard]]
        std::vector<
            domain::validation::PairValidationResult
        > allResults() const;

        [[nodiscard]]
        std::size_t totalSelected() const noexcept;

    private:
        void validate() const;

        std::vector<
            domain::validation::PairValidationResult
        > automaticResults_;

        std::vector<
            domain::validation::PairValidationResult
        > explicitResults_;
    };

} // namespace qart::core::visualization

#endif //VISUAL_THERMAL_CONCEPT_PREVIEW_SELECTION_H