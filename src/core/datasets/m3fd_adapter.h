//
// Created by hakgu on 10/9/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_M3FD_ADAPTER_H
#define VISUAL_THERMAL_CONCEPT_M3FD_ADAPTER_H

#pragma once
#include "dataset_adapter.h"
#include<filesystem>
#include<string>
#include<vector>

namespace qart::core::datasets {
    class M3FDAdapter : public DatasetAdapter {
    public:
        [[nodiscard]]
        std::string datasetId() const override;
        [[nodiscard]]
        std::vector<domain::datasets::DatasetPartitionId> supportedPartitions() const override;
        [[nodiscard]]
        std::vector<domain::datasets::DatasetPair> discoverPairs(const std::filesystem::path &datasetRoot, const domain::datasets::DatasetPartitionId &partitionId) const override;

        void validateLayout(const std::filesystem::path &datasetRoot) const override;
    private:
        [[nodiscard]]
        static std::filesystem::path datasetDirectory(
            const std::filesystem::path& datasetRoot
        );

        [[nodiscard]]
        static std::filesystem::path modalityDirectory(
            const std::filesystem::path& datasetRoot,
            const std::string& modalityDirectoryName
        );

        [[nodiscard]]
        static std::string toManifestRelativePath(
            const std::filesystem::path& physicalPath,
            const std::filesystem::path& datasetRoot
        );
    };
}

#endif //VISUAL_THERMAL_CONCEPT_M3FD_ADAPTER_H