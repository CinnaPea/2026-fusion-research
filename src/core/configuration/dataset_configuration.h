//
// Created by hakgu on 8/17/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_DATASET_CONFIGURATION_H
#define VISUAL_THERMAL_CONCEPT_DATASET_CONFIGURATION_H

#pragma once

#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/validation/pair_validation_policy.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace qart::core::configuration {

inline constexpr std::string_view
    kDatasetConfigurationSchemaVersion = "2.0";

class DatasetConfiguration final
{
public:
    DatasetConfiguration(
        std::string schemaVersion,
        std::string datasetId,
        std::filesystem::path datasetRoot,
        std::string manifestRelativePath,
        std::vector<domain::datasets::DatasetPartitionId>
            supportedPartitions,
        domain::validation::PairValidationPolicy
            validationPolicy
    );

    [[nodiscard]]
    const std::string& schemaVersion() const noexcept;

    [[nodiscard]]
    const std::string& datasetId() const noexcept;

    [[nodiscard]]
    const std::filesystem::path&
    datasetRoot() const noexcept;

    [[nodiscard]]
    const std::string&
    manifestRelativePath() const noexcept;

    [[nodiscard]]
    const std::vector<
        domain::datasets::DatasetPartitionId
    >& supportedPartitions() const noexcept;

    [[nodiscard]]
    const domain::validation::PairValidationPolicy&
    validationPolicy() const noexcept;

    [[nodiscard]]
    bool supportsPartition(
        const domain::datasets::DatasetPartitionId& partitionId
    ) const noexcept;

    [[nodiscard]]
    bool supportsPartition(
        const std::string& partitionId
    ) const;

    [[nodiscard]]
    std::filesystem::path resolveManifestPath() const;

    friend bool operator==(
        const DatasetConfiguration& left,
        const DatasetConfiguration& right
    ) noexcept;

    friend bool operator!=(
        const DatasetConfiguration& left,
        const DatasetConfiguration& right
    ) noexcept;

private:
    void validate() const;

    std::string schemaVersion_;
    std::string datasetId_;
    std::filesystem::path datasetRoot_;
    std::string manifestRelativePath_;

    std::vector<
        domain::datasets::DatasetPartitionId
    > supportedPartitions_;

    domain::validation::PairValidationPolicy
        validationPolicy_;
};

} // namespace qart::core::configuration

#endif //VISUAL_THERMAL_CONCEPT_DATASET_CONFIGURATION_H