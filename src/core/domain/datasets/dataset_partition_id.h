//
// Created by hakgu on 8/7/2026.
//
#pragma once

#include<string>


#ifndef VISUAL_THERMAL_CONCEPT_DATASET_PARTITION_ID_H
#define VISUAL_THERMAL_CONCEPT_DATASET_PARTITION_ID_H

namespace qart::core::domain::datasets {
    class DatasetPartitionId final {
    public:
        explicit DatasetPartitionId(std::string val);

        [[nodiscard]]
        const std::string& value() const noexcept;

        friend bool operator==(const DatasetPartitionId& lhs, const DatasetPartitionId& rhs) noexcept;
        friend bool operator!=(const DatasetPartitionId& lhs, const DatasetPartitionId& rhs) noexcept;
        friend bool operator<(const DatasetPartitionId& lhs, const DatasetPartitionId& rhs) noexcept;
        friend bool operator>(const DatasetPartitionId& lhs, const DatasetPartitionId& rhs) noexcept;
        friend bool operator<=(const DatasetPartitionId& lhs, const DatasetPartitionId& rhs) noexcept;
        friend bool operator>=(const DatasetPartitionId& lhs, const DatasetPartitionId& rhs) noexcept;

    private:
        static bool isLowercaseLetterOrDigit(char ch) noexcept;
        static void validate(const std::string& val);
        std::string value_;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_DATASET_PARTITION_ID_H