//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include <string>
#include <vector>
#include "dataset_partition_id.h"
#include "dataset_usage_role.h"
#include "experiment_view_assignment.h"

#ifndef VISUAL_THERMAL_CONCEPT_EXPERIMENT_VIEW_H
#define VISUAL_THERMAL_CONCEPT_EXPERIMENT_VIEW_H

namespace qart::core::domain::datasets {
    class ExperimentView final
    {
    public:
        ExperimentView(
            std::string viewId,
            std::vector<ExperimentViewAssignment> assignments
        );

        [[nodiscard]]
        const std::string& viewId() const noexcept;

        [[nodiscard]]
        const std::vector<ExperimentViewAssignment>&
        assignments() const noexcept;

        [[nodiscard]]
        DatasetUsageRole roleFor(
            const std::string& datasetId,
            const DatasetPartitionId& partitionId
        ) const;

        [[nodiscard]]
        DatasetUsageRole roleFor(
            const std::string& datasetId,
            const std::string& partitionId
        ) const;

    private:
        static void validateViewId(
            const std::string& viewId
        );

        static void validateUniqueAssignments(
            const std::vector<ExperimentViewAssignment>& assignments
        );

        std::string viewId_;
        std::vector<ExperimentViewAssignment> assignments_;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_EXPERIMENT_VIEW_H