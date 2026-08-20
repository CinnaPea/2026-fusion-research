//
// Created by hakgu on 8/11/2026.
//

#include "pair_validation_result.h"
#include <stdexcept>
#include <utility>

namespace qart::core::domain::validation {
    PairValidationResult::PairValidationResult(
    datasets::DatasetPair pair,
    const PairValidationStatus status,
    const bool visibleDecoded,
    const bool thermalDecoded,
    std::optional<imaging::ImageDimensions> visibleDimensions,
    std::optional<imaging::ImageDimensions> thermalDimensions,
    std::optional<std::string> message
    )
        : pair_(std::move(pair)),
          status_(status),
          visibleDecoded_(visibleDecoded),
          thermalDecoded_(thermalDecoded),
          visibleDimensions_(std::move(visibleDimensions)),
          thermalDimensions_(std::move(thermalDimensions)),
          message_(std::move(message))
    {
        validate();
    }

    const datasets::DatasetPair&
    PairValidationResult::pair() const noexcept
    {
        return pair_;
    }

    PairValidationStatus
    PairValidationResult::status() const noexcept
    {
        return status_;
    }

    bool PairValidationResult::visibleDecoded() const noexcept
    {
        return visibleDecoded_;
    }

    bool PairValidationResult::thermalDecoded() const noexcept
    {
        return thermalDecoded_;
    }

    const std::optional<imaging::ImageDimensions>&
    PairValidationResult::visibleDimensions() const noexcept
    {
        return visibleDimensions_;
    }

    const std::optional<imaging::ImageDimensions>&
    PairValidationResult::thermalDimensions() const noexcept
    {
        return thermalDimensions_;
    }

    const std::optional<std::string>&
    PairValidationResult::message() const noexcept
    {
        return message_;
    }

    bool PairValidationResult::dimensionsMatch() const noexcept
    {
        return visibleDimensions_.has_value()
            && thermalDimensions_.has_value()
            && visibleDimensions_ == thermalDimensions_;
    }

    bool PairValidationResult::isValid() const noexcept
    {
        return status_ == PairValidationStatus::Valid;
    }

    void PairValidationResult::validate() const
    {
        if (
            visibleDecoded_
            != visibleDimensions_.has_value()
        ) {
            throw std::invalid_argument(
                "visible_decoded must be true exactly when "
                "visible_dimensions is available."
            );
        }

        if (
            thermalDecoded_
            != thermalDimensions_.has_value()
        ) {
            throw std::invalid_argument(
                "thermal_decoded must be true exactly when "
                "thermal_dimensions is available."
            );
        }

        if (status_ == PairValidationStatus::Valid) {
            if (!visibleDecoded_ || !thermalDecoded_) {
                throw std::invalid_argument(
                    "A VALID pair must have both source images decoded."
                );
            }

            if (!dimensionsMatch()) {
                throw std::invalid_argument(
                    "A VALID pair must have matching visible and thermal "
                    "dimensions."
                );
            }
        }
    }

    bool operator==(
        const PairValidationResult& left,
        const PairValidationResult& right
    ) noexcept
    {
        return left.pair_ == right.pair_
            && left.status_ == right.status_
            && left.visibleDecoded_ == right.visibleDecoded_
            && left.thermalDecoded_ == right.thermalDecoded_
            && left.visibleDimensions_ == right.visibleDimensions_
            && left.thermalDimensions_ == right.thermalDimensions_
            && left.message_ == right.message_;
    }

    bool operator!=(
        const PairValidationResult& left,
        const PairValidationResult& right
    ) noexcept
    {
        return !(left == right);
    }
}