//
// Created by hakgu on 8/19/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_LLVIP_ADAPTER_H
#define VISUAL_THERMAL_CONCEPT_LLVIP_ADAPTER_H

#pragma once

#include "core/datasets/dataset_adapter.h"

#include <filesystem>
#include <string>
#include <vector>

namespace qart::core::datasets {

    class LLVIPAdapter final : public DatasetAdapter
    {
    public:
        [[nodiscard]]
        std::string datasetId() const override;

        [[nodiscard]]
        std::vector<
            domain::datasets::DatasetPartitionId
        > supportedPartitions() const override;

        void validateLayout(
            const std::filesystem::path& datasetRoot
        ) const override;

        [[nodiscard]]
        std::vector<
            domain::datasets::DatasetPair
        > discoverPairs(
            const std::filesystem::path& datasetRoot,
            const domain::datasets::DatasetPartitionId&
                partitionId
        ) const override;

    private:
        [[nodiscard]]
        static std::filesystem::path datasetDirectory(
            const std::filesystem::path& datasetRoot
        );

        [[nodiscard]]
        static std::filesystem::path partitionDirectory(
            const std::filesystem::path& datasetRoot,
            const std::string& modalityDirectoryName,
            const domain::datasets::DatasetPartitionId&
                partitionId
        );

        [[nodiscard]]
        static std::string toManifestRelativePath(
            const std::filesystem::path& physicalPath,
            const std::filesystem::path& datasetRoot
        );
    };

} // namespace qart::core::datasets

#endif //VISUAL_THERMAL_CONCEPT_LLVIP_ADAPTER_H