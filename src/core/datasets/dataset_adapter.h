//
// Created by hakgu on 8/19/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_DATASET_ADAPTER_H
#define VISUAL_THERMAL_CONCEPT_DATASET_ADAPTER_H

#pragma once

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <string>
#include <vector>

namespace qart::core::datasets {

    class DatasetAdapter
    {
    public:
        virtual ~DatasetAdapter() = default;

        DatasetAdapter(
            const DatasetAdapter&
        ) = default;

        DatasetAdapter& operator=(
            const DatasetAdapter&
        ) = default;

        DatasetAdapter(
            DatasetAdapter&&
        ) noexcept = default;

        DatasetAdapter& operator=(
            DatasetAdapter&&
        ) noexcept = default;

        [[nodiscard]]
        virtual std::string datasetId() const = 0;

        [[nodiscard]]
        virtual std::vector<
            domain::datasets::DatasetPartitionId
        > supportedPartitions() const = 0;

        [[nodiscard]]
        bool supportsPartition(
            const domain::datasets::DatasetPartitionId&
                partitionId
        ) const;

        [[nodiscard]]
        bool supportsPartition(
            const std::string& partitionId
        ) const;

        virtual void validateLayout(
            const std::filesystem::path& datasetRoot
        ) const = 0;

        [[nodiscard]]
        virtual std::vector<
            domain::datasets::DatasetPair
        > discoverPairs(
            const std::filesystem::path& datasetRoot,
            const domain::datasets::DatasetPartitionId&
                partitionId
        ) const = 0;

    protected:
        DatasetAdapter() = default;
    };

} // namespace qart::core::datasets

#endif //VISUAL_THERMAL_CONCEPT_DATASET_ADAPTER_H