//
// Created by hakgu on 8/19/2026.
//

#include "core/datasets/dataset_adapter.h"

#include <algorithm>

namespace qart::core::datasets {

    bool DatasetAdapter::supportsPartition(
        const domain::datasets::DatasetPartitionId&
            partitionId
    ) const
    {
        const auto partitions =
            supportedPartitions();

        return std::find(
            partitions.begin(),
            partitions.end(),
            partitionId
        ) != partitions.end();
    }

    bool DatasetAdapter::supportsPartition(
        const std::string& partitionId
    ) const
    {
        return supportsPartition(
            domain::datasets::DatasetPartitionId(
                partitionId
            )
        );
    }

} // namespace qart::core::datasets