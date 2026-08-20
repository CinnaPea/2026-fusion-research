//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include "pair_validation_status.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/imaging/image_dimensions.h"
#include <optional>
#include <string>

#ifndef VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_RESULT_H
#define VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_RESULT_H

namespace qart::core::domain::validation {

    class PairValidationResult final
    {
    public:
        PairValidationResult(
            datasets::DatasetPair pair,
            PairValidationStatus status,
            bool visibleDecoded,
            bool thermalDecoded,
            std::optional<imaging::ImageDimensions> visibleDimensions =
                std::nullopt,
            std::optional<imaging::ImageDimensions> thermalDimensions =
                std::nullopt,
            std::optional<std::string> message =
                std::nullopt
        );

        [[nodiscard]]
        const datasets::DatasetPair& pair() const noexcept;

        [[nodiscard]]
        PairValidationStatus status() const noexcept;

        [[nodiscard]]
        bool visibleDecoded() const noexcept;

        [[nodiscard]]
        bool thermalDecoded() const noexcept;

        [[nodiscard]]
        const std::optional<imaging::ImageDimensions>&
        visibleDimensions() const noexcept;

        [[nodiscard]]
        const std::optional<imaging::ImageDimensions>&
        thermalDimensions() const noexcept;

        [[nodiscard]]
        const std::optional<std::string>& message() const noexcept;

        [[nodiscard]]
        bool dimensionsMatch() const noexcept;

        [[nodiscard]]
        bool isValid() const noexcept;

        friend bool operator==(
            const PairValidationResult& left,
            const PairValidationResult& right
        ) noexcept;

        friend bool operator!=(
            const PairValidationResult& left,
            const PairValidationResult& right
        ) noexcept;

    private:
        void validate() const;

        datasets::DatasetPair pair_;
        PairValidationStatus status_;

        bool visibleDecoded_;
        bool thermalDecoded_;

        std::optional<imaging::ImageDimensions> visibleDimensions_;
        std::optional<imaging::ImageDimensions> thermalDimensions_;

        std::optional<std::string> message_;
    };

}

#endif //VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_RESULT_H