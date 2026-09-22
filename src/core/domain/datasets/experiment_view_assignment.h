//
// Created by hakgu on 8/10/2026.
//
#pragma once

#include "dataset_partition_id.h"
#include "dataset_usage_role.h"
#include<string>

#ifndef VISUAL_THERMAL_CONCEPT_EXPERIMENT_VIEW_ASSIGNMENT_H
#define VISUAL_THERMAL_CONCEPT_EXPERIMENT_VIEW_ASSIGNMENT_H

namespace qart::core::domain::datasets {

    class ExperimentViewAssignment final
    {
    public:
        ExperimentViewAssignment(
            std::string datasetId,
            DatasetPartitionId partitionId,
            DatasetUsageRole usageRole
        );

        [[nodiscard]]
        const std::string& datasetId() const noexcept;

        [[nodiscard]]
        const DatasetPartitionId& partitionId() const noexcept;

        [[nodiscard]]
        DatasetUsageRole usageRole() const noexcept;

        friend bool operator==(
            const ExperimentViewAssignment& left,
            const ExperimentViewAssignment& right
        ) noexcept;

        friend bool operator!=(
            const ExperimentViewAssignment& left,
            const ExperimentViewAssignment& right
        ) noexcept;

    private:
        static void validateDatasetId(
            const std::string& datasetId
        );

        std::string datasetId_;
        DatasetPartitionId partitionId_;
        DatasetUsageRole usageRole_;
    };

} // namespace qart::core::domain::datasets

#endif //VISUAL_THERMAL_CONCEPT_EXPERIMENT_VIEW_ASSIGNMENT_H