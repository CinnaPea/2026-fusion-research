//
// Created by hakgu on 8/10/2026.
//

#include "core/domain/datasets/dataset_usage_role.h"

#include<stdexcept>
#include<string>
#include<vector>

namespace {
    using qart::core::domain::datasets::DatasetUsageRole;
    using qart::core::domain::datasets::datasetUsageRoleFromString;
    using qart::core::domain::datasets::toString;

    struct UsageRoleCase {
        DatasetUsageRole role;
        std::string txt;
    };

    bool rejectsInvalidRole(const std::string& val) {
        try {
            const DatasetUsageRole role = datasetUsageRoleFromString(val);
            static_cast<void>(role);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    }
}

int main() {
    const std::vector<UsageRoleCase> valids = {
        {DatasetUsageRole::Benchmark, "benchmark"},
        {DatasetUsageRole::Validation, "validation"},
        {DatasetUsageRole::Testing, "testing"},
        {DatasetUsageRole::Training, "training"},
        {DatasetUsageRole::DetectionEvaluation, "detection_evaluation"}
    };

    for (const UsageRoleCase& role : valids) {
        if (toString(role.role) != role.txt) return 1;
        if (datasetUsageRoleFromString(role.txt) != role.role) return 2;
    }
    const std::vector<std::string> invalids = {
        "",
        "Training",
        " training",
        "training ",
        "detection-evaluation",
        "benchmark_test",
        "unknown"
    };

    for (const std::string& invalid : invalids) {
        if (!rejectsInvalidRole(invalid)) return 3;
    }
    return 0;
}

