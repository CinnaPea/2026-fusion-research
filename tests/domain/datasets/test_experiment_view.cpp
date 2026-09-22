//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/datasets/experiment_view.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::datasets::DatasetUsageRole;
using qart::core::domain::datasets::ExperimentView;
using qart::core::domain::datasets::ExperimentViewAssignment;

ExperimentViewAssignment roadsceneAssignment()
{
    return ExperimentViewAssignment(
        "roadscene",
        DatasetPartitionId("all"),
        DatasetUsageRole::Benchmark
    );
}

bool rejectsDuplicateAssignment()
{
    try {
        const ExperimentViewAssignment assignment =
            roadsceneAssignment();

        const ExperimentView view(
            "duplicate_view",
            {
                assignment,
                assignment
            }
        );

        static_cast<void>(view);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsInvalidViewId(const std::string& viewId)
{
    try {
        const ExperimentView view(
            viewId,
            {}
        );

        static_cast<void>(view);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsMissingAssignment()
{
    try {
        const ExperimentView view(
            "roadscene_benchmark",
            {
                roadsceneAssignment()
            }
        );

        static_cast<void>(
            view.roleFor("roadscene", "train")
        );
    }
    catch (const std::out_of_range&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const ExperimentView view(
        "roadscene_benchmark",
        {
            roadsceneAssignment()
        }
    );

    if (view.viewId() != "roadscene_benchmark") {
        return 1;
    }

    if (view.assignments().size() != 1) {
        return 2;
    }

    if (
        view.roleFor("roadscene", "all")
        != DatasetUsageRole::Benchmark
    ) {
        return 3;
    }

    if (
        view.roleFor(
            "roadscene",
            DatasetPartitionId("all")
        )
        != DatasetUsageRole::Benchmark
    ) {
        return 4;
    }

    if (!rejectsDuplicateAssignment()) {
        return 5;
    }

    const std::vector<std::string> invalidViewIds = {
        "",
        "RoadScene Benchmark",
        "roadscene benchmark",
        "roadscene__benchmark",
        "_roadscene",
        "roadscene_"
    };

    for (const std::string& invalidViewId : invalidViewIds) {
        if (!rejectsInvalidViewId(invalidViewId)) {
            return 6;
        }
    }

    if (!rejectsMissingAssignment()) {
        return 7;
    }

    return 0;
}