//
// Created by hakgu on 8/10/2026.
//

#include "core/domain/datasets/experiment_view_assignment.h"

#include <stdexcept>
#include <utility>

namespace qart::core::domain::datasets {

namespace {

bool isAsciiUppercase(const char character) noexcept
{
    return character >= 'A' && character <= 'Z';
}

bool isAsciiWhitespace(const char character) noexcept
{
    return character == ' '
        || character == '\t'
        || character == '\n'
        || character == '\r'
        || character == '\f'
        || character == '\v';
}

} // namespace

ExperimentViewAssignment::ExperimentViewAssignment(
    std::string datasetId,
    DatasetPartitionId partitionId,
    const DatasetUsageRole usageRole
)
    : datasetId_(std::move(datasetId)),
      partitionId_(std::move(partitionId)),
      usageRole_(usageRole)
{
    validateDatasetId(datasetId_);
}

const std::string&
ExperimentViewAssignment::datasetId() const noexcept
{
    return datasetId_;
}

const DatasetPartitionId&
ExperimentViewAssignment::partitionId() const noexcept
{
    return partitionId_;
}

DatasetUsageRole
ExperimentViewAssignment::usageRole() const noexcept
{
    return usageRole_;
}

void ExperimentViewAssignment::validateDatasetId(
    const std::string& datasetId
)
{
    if (datasetId.empty()) {
        throw std::invalid_argument(
            "Experiment-view dataset_id must not be empty."
        );
    }

    if (
        isAsciiWhitespace(datasetId.front())
        || isAsciiWhitespace(datasetId.back())
    ) {
        throw std::invalid_argument(
            "Experiment-view dataset_id must be lowercase without "
            "surrounding whitespace."
        );
    }

    for (const char character : datasetId) {
        if (isAsciiUppercase(character)) {
            throw std::invalid_argument(
                "Experiment-view dataset_id must be lowercase without "
                "surrounding whitespace."
            );
        }
    }
}

bool operator==(
    const ExperimentViewAssignment& left,
    const ExperimentViewAssignment& right
) noexcept
{
    return left.datasetId_ == right.datasetId_
        && left.partitionId_ == right.partitionId_
        && left.usageRole_ == right.usageRole_;
}

bool operator!=(
    const ExperimentViewAssignment& left,
    const ExperimentViewAssignment& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::domain::datasets