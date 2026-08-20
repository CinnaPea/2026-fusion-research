//
// Created by hakgu on 8/7/2026.
//
#pragma once
#include <string>
#include <string_view>

#ifndef VISUAL_THERMAL_CONCEPT_DATASET_USAGE_ROLE_H
#define VISUAL_THERMAL_CONCEPT_DATASET_USAGE_ROLE_H

namespace qart::core::domain::datasets {
    enum class DatasetUsageRole {
        Training,
        Validation,
        Testing,
        Benchmark,
        DetectionEvaluation
    };

    [[nodiscard]]
    std::string_view toString(DatasetUsageRole role) noexcept;

    [[nodiscard]]
    DatasetUsageRole datasetUsageRoleFromString(const std::string& val);
}

#endif //VISUAL_THERMAL_CONCEPT_DATASET_USAGE_ROLE_H