//
// Created by hakgu on 8/7/2026.
//

#include "core/domain/datasets/dataset_partition_id.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using qart::core::domain::datasets::DatasetPartitionId;
    bool rejectsInvalidPartition(const std::string& value)
    {
        try {
            const DatasetPartitionId partition(value);
        }
        catch (const std::invalid_argument&) {
            return true;
        }

        return false;
    }

} // namespace

int main()
{
    const DatasetPartitionId train("train");
    const DatasetPartitionId all("all");
    const DatasetPartitionId sequence("sequence_01");

    if (train.value() != "train") {
        return 1;
    }

    if (all.value() != "all") {
        return 2;
    }

    if (sequence.value() != "sequence_01") {
        return 3;
    }

    const std::vector<std::string> invalidValues = {
        "",
        " Train",
        "TRAIN",
        "train/test",
        "train.test",
        "train__test"
    };

    for (const std::string& invalidValue : invalidValues) {
        if (!rejectsInvalidPartition(invalidValue)) {
            return 4;
        }
    }

    if (DatasetPartitionId("train") != DatasetPartitionId("train")) {
        return 5;
    }

    if (DatasetPartitionId("train") == DatasetPartitionId("test")) {
        return 6;
    }

    if (DatasetPartitionId("all") >= DatasetPartitionId("train")) {
        return 7;
    }

    if (DatasetPartitionId("train") <= DatasetPartitionId("all")) {
        return 8;
    }

    return 0;
}