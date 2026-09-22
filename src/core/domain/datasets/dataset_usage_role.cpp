//
// Created by hakgu on 8/7/2026.
//

#include "dataset_usage_role.h"

#include<stdexcept>

namespace qart::core::domain::datasets {
    std::string_view toString(const DatasetUsageRole role) noexcept {
        switch (role) {
            case DatasetUsageRole::Benchmark:
                return "benchmark";
            case DatasetUsageRole::DetectionEvaluation:
                return "detection_evaluation";
            case DatasetUsageRole::Testing:
                return "testing";
            case DatasetUsageRole::Validation:
                return "validation";
            case DatasetUsageRole::Training:
                return "training";
        }
        return "";
    }

    DatasetUsageRole datasetUsageRoleFromString(const std::string& val) {
        if (val == "benchmark") return DatasetUsageRole::Benchmark;
        if (val == "testing") return DatasetUsageRole::Testing;
        if (val == "training") return DatasetUsageRole::Training;
        if (val == "validation") return DatasetUsageRole::Validation;
        if (val == "detection_evaluation") return DatasetUsageRole::DetectionEvaluation;
        throw std::invalid_argument("Invalid dataset usage role: " + val);
    }
}