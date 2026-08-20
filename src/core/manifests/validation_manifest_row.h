//
// Created by hakgu on 8/18/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_VALIDATION_MANIFEST_ROW_H
#define VISUAL_THERMAL_CONCEPT_VALIDATION_MANIFEST_ROW_H

#pragma once

#include "core/domain/validation/pair_validation_result.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace qart::core::manifests {

inline constexpr std::string_view
    kValidationManifestSchemaVersion = "2.0";

inline constexpr std::size_t
    kValidationManifestColumnCount = 15;

inline constexpr std::array<
    std::string_view,
    kValidationManifestColumnCount
> kValidationManifestColumns = {
    "schema_version",
    "pair_id",
    "dataset_id",
    "partition_id",
    "source_stem",
    "visible_relative_path",
    "thermal_relative_path",
    "status",
    "visible_decoded",
    "thermal_decoded",
    "visible_width",
    "visible_height",
    "thermal_width",
    "thermal_height",
    "message"
};

class ValidationManifestRow final
{
public:
    ValidationManifestRow(
        std::string pairId,
        std::string datasetId,
        std::string partitionId,
        std::string sourceStem,
        std::string visibleRelativePath,
        std::string thermalRelativePath,
        std::string status,
        bool visibleDecoded,
        bool thermalDecoded,
        std::optional<int> visibleWidth = std::nullopt,
        std::optional<int> visibleHeight = std::nullopt,
        std::optional<int> thermalWidth = std::nullopt,
        std::optional<int> thermalHeight = std::nullopt,
        std::optional<std::string> message = std::nullopt
    );

    [[nodiscard]]
    static ValidationManifestRow fromResult(
        const domain::validation::PairValidationResult& result
    );

    [[nodiscard]]
    domain::validation::PairValidationResult
    toResult() const;

    [[nodiscard]]
    std::array<
        std::string,
        kValidationManifestColumnCount
    > toCsvValues() const;

    [[nodiscard]]
    const std::string& pairId() const noexcept;

    [[nodiscard]]
    const std::string& datasetId() const noexcept;

    [[nodiscard]]
    const std::string& partitionId() const noexcept;

    [[nodiscard]]
    const std::string& sourceStem() const noexcept;

    [[nodiscard]]
    const std::string&
    visibleRelativePath() const noexcept;

    [[nodiscard]]
    const std::string&
    thermalRelativePath() const noexcept;

    [[nodiscard]]
    const std::string& status() const noexcept;

    [[nodiscard]]
    bool visibleDecoded() const noexcept;

    [[nodiscard]]
    bool thermalDecoded() const noexcept;

    [[nodiscard]]
    const std::optional<int>&
    visibleWidth() const noexcept;

    [[nodiscard]]
    const std::optional<int>&
    visibleHeight() const noexcept;

    [[nodiscard]]
    const std::optional<int>&
    thermalWidth() const noexcept;

    [[nodiscard]]
    const std::optional<int>&
    thermalHeight() const noexcept;

    [[nodiscard]]
    const std::optional<std::string>&
    message() const noexcept;

    friend bool operator==(
        const ValidationManifestRow& left,
        const ValidationManifestRow& right
    ) noexcept;

    friend bool operator!=(
        const ValidationManifestRow& left,
        const ValidationManifestRow& right
    ) noexcept;

private:
    static void validateDimensionPair(
        const std::optional<int>& width,
        const std::optional<int>& height,
        std::string_view modalityName
    );

    void validate() const;

    std::string pairId_;
    std::string datasetId_;
    std::string partitionId_;

    std::string sourceStem_;

    std::string visibleRelativePath_;
    std::string thermalRelativePath_;

    std::string status_;

    bool visibleDecoded_;
    bool thermalDecoded_;

    std::optional<int> visibleWidth_;
    std::optional<int> visibleHeight_;

    std::optional<int> thermalWidth_;
    std::optional<int> thermalHeight_;

    std::optional<std::string> message_;
};

} // namespace qart::core::manifests

#endif //VISUAL_THERMAL_CONCEPT_VALIDATION_MANIFEST_ROW_H