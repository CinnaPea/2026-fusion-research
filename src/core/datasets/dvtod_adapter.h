#pragma once

#include "dataset_adapter.h"

#include <filesystem>
#include <string>
#include <vector>

#ifndef VISUAL_THERMAL_CONCEPT_DVTOD_ADAPTER_H
#define VISUAL_THERMAL_CONCEPT_DVTOD_ADAPTER_H

namespace qart::core::datasets {

class DVTODAdapter final : public DatasetAdapter
{
public:
    [[nodiscard]]
    std::string datasetId() const override;

    [[nodiscard]]
    std::vector<domain::datasets::DatasetPartitionId>
    supportedPartitions() const override;

    [[nodiscard]]
    std::vector<domain::datasets::DatasetPair>
    discoverPairs(
        const std::filesystem::path& datasetRoot,
        const domain::datasets::DatasetPartitionId& partitionId
    ) const override;

    void validateLayout(
        const std::filesystem::path& datasetRoot
    ) const override;

private:
    [[nodiscard]]
    static std::filesystem::path datasetDirectory(
        const std::filesystem::path& datasetRoot
    );

    [[nodiscard]]
    static std::filesystem::path partitionDirectory(
        const std::filesystem::path& datasetRoot,
        const std::string& modalityRoot,
        const domain::datasets::DatasetPartitionId& partitionId
    );

    [[nodiscard]]
    static std::string toManifestRelativePath(
        const std::filesystem::path& physicalPath,
        const std::filesystem::path& datasetRoot
    );
};

} // namespace qart::core::datasets

#endif
