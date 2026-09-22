//
// Created by hakgu on 8/14/2026.
//
#pragma once

#include "core/domain/imaging/image_dimensions.h"
#include<optional>
#include <string>
#include <vector>

#ifndef VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_POLICY_H
#define VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_POLICY_H

namespace qart::core::domain::validation {

    class PairValidationPolicy final
    {
    public:
        PairValidationPolicy(
            std::vector<std::string> supportedVisibleExtensions,
            std::vector<std::string> supportedThermalExtensions,
            std::optional<imaging::ImageDimensions>
                expectedVisibleDimensions = std::nullopt,
            std::optional<imaging::ImageDimensions>
                expectedThermalDimensions = std::nullopt
        );

        [[nodiscard]]
        const std::vector<std::string>&
        supportedVisibleExtensions() const noexcept;

        [[nodiscard]]
        const std::vector<std::string>&
        supportedThermalExtensions() const noexcept;

        [[nodiscard]]
        const std::optional<imaging::ImageDimensions>&
        expectedVisibleDimensions() const noexcept;

        [[nodiscard]]
        const std::optional<imaging::ImageDimensions>&
        expectedThermalDimensions() const noexcept;

        friend bool operator==(
            const PairValidationPolicy& left,
            const PairValidationPolicy& right
        ) noexcept;

        friend bool operator!=(
            const PairValidationPolicy& left,
            const PairValidationPolicy& right
        ) noexcept;

    private:
        [[nodiscard]]
        static std::vector<std::string> normalizeExtensions(
            const std::vector<std::string>& extensions,
            const char* fieldName
        );

        std::vector<std::string> supportedVisibleExtensions_;
        std::vector<std::string> supportedThermalExtensions_;

        std::optional<imaging::ImageDimensions>
            expectedVisibleDimensions_;

        std::optional<imaging::ImageDimensions>
            expectedThermalDimensions_;
    };

}

#endif //VISUAL_THERMAL_CONCEPT_PAIR_VALIDATION_POLICY_H