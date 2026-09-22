//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include "dataset_partition_id.h"
#include<string>

#ifndef VISUAL_THERMAL_CONCEPT_DATASET_PAIR_H
#define VISUAL_THERMAL_CONCEPT_DATASET_PAIR_H

namespace qart::core::domain::datasets {

    class DatasetPair final
    {
    public:
        DatasetPair(
            std::string pairId,
            std::string datasetId,
            DatasetPartitionId partitionId,
            std::string sourceStem,
            std::string visibleRelativePath,
            std::string thermalRelativePath
        );

        [[nodiscard]]
        const std::string& pairId() const noexcept;

        [[nodiscard]]
        const std::string& datasetId() const noexcept;

        [[nodiscard]]
        const DatasetPartitionId& partitionId() const noexcept;

        [[nodiscard]]
        const std::string& sourceStem() const noexcept;

        [[nodiscard]]
        const std::string& visibleRelativePath() const noexcept;

        [[nodiscard]]
        const std::string& thermalRelativePath() const noexcept;

        friend bool operator==(
            const DatasetPair& left,
            const DatasetPair& right
        ) noexcept;

        friend bool operator!=(
            const DatasetPair& left,
            const DatasetPair& right
        ) noexcept;

    private:
        static void validateRequiredText(
            const std::string& value,
            const char* fieldName
        );

        static void validateRelativeManifestPath(
            const std::string& path,
            const char* fieldName
        );

        std::string pairId_;
        std::string datasetId_;
        DatasetPartitionId partitionId_;
        std::string sourceStem_;
        std::string visibleRelativePath_;
        std::string thermalRelativePath_;
    };

}

#endif //VISUAL_THERMAL_CONCEPT_DATASET_PAIR_H