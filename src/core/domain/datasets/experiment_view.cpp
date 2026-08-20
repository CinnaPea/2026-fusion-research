//
// Created by hakgu on 8/11/2026.
//

#include "experiment_view.h"
#include<set>
#include<stdexcept>
#include<utility>

namespace qart::core::domain::datasets {

namespace {

bool isLowercaseLetterOrDigit(
    const char character
) noexcept
{
    const bool isLowercase =
        character >= 'a' && character <= 'z';

    const bool isDigit =
        character >= '0' && character <= '9';

    return isLowercase || isDigit;
}

} // namespace

ExperimentView::ExperimentView(
    std::string viewId,
    std::vector<ExperimentViewAssignment> assignments
)
    : viewId_(std::move(viewId)),
      assignments_(std::move(assignments))
{
    validateViewId(viewId_);
    validateUniqueAssignments(assignments_);
}

const std::string&
ExperimentView::viewId() const noexcept
{
    return viewId_;
}

const std::vector<ExperimentViewAssignment>&
ExperimentView::assignments() const noexcept
{
    return assignments_;
}

void ExperimentView::validateViewId(
    const std::string& viewId
)
{
    if (viewId.empty()) {
        throw std::invalid_argument(
            "Experiment view_id must be a canonical lowercase "
            "identifier."
        );
    }

    for (std::size_t index = 0; index < viewId.size(); ++index) {
        const char current = viewId[index];

        if (isLowercaseLetterOrDigit(current)) {
            continue;
        }

        const bool isSeparator =
            current == '_' || current == '-';

        if (!isSeparator) {
            throw std::invalid_argument(
                "Experiment view_id must be a canonical lowercase "
                "identifier."
            );
        }

        const bool hasPreviousCharacter = index > 0;
        const bool hasNextCharacter =
            index + 1 < viewId.size();

        if (
            !hasPreviousCharacter
            || !hasNextCharacter
        ) {
            throw std::invalid_argument(
                "Experiment view_id must be a canonical lowercase "
                "identifier."
            );
        }

        const char previous = viewId[index - 1];
        const char next = viewId[index + 1];

        if (
            !isLowercaseLetterOrDigit(previous)
            || !isLowercaseLetterOrDigit(next)
        ) {
            throw std::invalid_argument(
                "Experiment view_id must be a canonical lowercase "
                "identifier."
            );
        }
    }
}

void ExperimentView::validateUniqueAssignments(
    const std::vector<ExperimentViewAssignment>& assignments
)
{
    std::set<
        std::pair<std::string, DatasetPartitionId>
    > assignmentKeys;

    for (const ExperimentViewAssignment& assignment : assignments) {
        const auto key = std::make_pair(
            assignment.datasetId(),
            assignment.partitionId()
        );

        const auto [iterator, inserted] =
            assignmentKeys.insert(key);

        static_cast<void>(iterator);

        if (!inserted) {
            throw std::invalid_argument(
                "Experiment view must not assign one dataset "
                "partition more than once."
            );
        }
    }
}

DatasetUsageRole ExperimentView::roleFor(
    const std::string& datasetId,
    const DatasetPartitionId& partitionId
) const
{
    for (const ExperimentViewAssignment& assignment : assignments_) {
        if (
            assignment.datasetId() == datasetId
            && assignment.partitionId() == partitionId
        ) {
            return assignment.usageRole();
        }
    }

    throw std::out_of_range(
        "Experiment view has no assignment for dataset='"
        + datasetId
        + "', partition='"
        + partitionId.value()
        + "'."
    );
}

DatasetUsageRole ExperimentView::roleFor(
    const std::string& datasetId,
    const std::string& partitionId
) const
{
    return roleFor(
        datasetId,
        DatasetPartitionId(partitionId)
    );
}

}