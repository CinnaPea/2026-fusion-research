//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/datasets/experiment_view_assignment.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

    using qart::core::domain::datasets::DatasetPartitionId;
    using qart::core::domain::datasets::DatasetUsageRole;
    using qart::core::domain::datasets::ExperimentViewAssignment;

    bool rejectsInvalidDatasetId(const std::string& datasetId)
    {
        try {
            const ExperimentViewAssignment assignment(
                datasetId,
                DatasetPartitionId("all"),
                DatasetUsageRole::Benchmark
            );

            static_cast<void>(assignment);
        }
        catch (const std::invalid_argument&) {
            return true;
        }

        return false;
    }

} // namespace

int main()
{
    const ExperimentViewAssignment assignment(
        "roadscene",
        DatasetPartitionId("all"),
        DatasetUsageRole::Benchmark
    );

    if (assignment.datasetId() != "roadscene") {
        return 1;
    }

    if (assignment.partitionId() != DatasetPartitionId("all")) {
        return 2;
    }

    if (assignment.usageRole() != DatasetUsageRole::Benchmark) {
        return 3;
    }

    const ExperimentViewAssignment equivalent(
        "roadscene",
        DatasetPartitionId("all"),
        DatasetUsageRole::Benchmark
    );

    if (assignment != equivalent) {
        return 4;
    }

    const ExperimentViewAssignment differentRole(
        "roadscene",
        DatasetPartitionId("all"),
        DatasetUsageRole::Testing
    );

    if (assignment == differentRole) {
        return 5;
    }

    const std::vector<std::string> invalidDatasetIds = {
        "",
        "RoadScene",
        " roadscene",
        "roadscene ",
        "\troadscene",
        "roadscene\n"
    };

    for (const std::string& invalidDatasetId : invalidDatasetIds) {
        if (!rejectsInvalidDatasetId(invalidDatasetId)) {
            return 6;
        }
    }

    return 0;
}