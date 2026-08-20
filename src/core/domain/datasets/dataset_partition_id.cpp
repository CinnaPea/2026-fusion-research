#include "dataset_partition_id.h"
#include <stdexcept>
#include <utility>

namespace qart::core::domain::datasets {
    DatasetPartitionId::DatasetPartitionId(std::string val) : value_(std::move(val)) {
        validate(value_);
    }
    const std::string &DatasetPartitionId::value() const noexcept {
        return value_;
    }
    bool DatasetPartitionId::isLowercaseLetterOrDigit(const char ch) noexcept {
        const bool isLowercase = ch >= 'a' && ch <= 'z';
        const bool isDigit = ch >= '0' && ch <= '9';
        return isLowercase || isDigit;
    }

    void DatasetPartitionId::validate(const std::string &val) {
        if (val.empty()) {
            throw std::invalid_argument("Dataset partition value must not be empty.");
        }
        for (std::size_t i = 0; i < val.size(); ++i) {
            const char cur = val[i];
            if (isLowercaseLetterOrDigit(cur)) continue;
            const bool isSeparator = cur == '_' || cur == '-';
            if (!isSeparator) throw std::invalid_argument("Dataset partition value contains a noncanonical character.");

            const bool hasPrevChar = i > 0, hasNextChar = i + 1 < val.size();
            if (!hasNextChar || !hasPrevChar) {
                throw std::invalid_argument("Dataset partition separator must be internal.");
            }

            const char prev = val[i-1], next = val[i+1];
            if (!isLowercaseLetterOrDigit(prev) || !isLowercaseLetterOrDigit(next)) {
                throw std::invalid_argument("Dataset partition separators must occur between lowercase letters or digits");
            }
        }
    }

    bool operator==(const DatasetPartitionId &lhs, const DatasetPartitionId &rhs) noexcept {
        return lhs.value_ == rhs.value_;
    }
    bool operator!=(const DatasetPartitionId &lhs, const DatasetPartitionId &rhs) noexcept {
        return !(lhs == rhs);
    }
    bool operator>(const DatasetPartitionId &lhs, const DatasetPartitionId &rhs) noexcept {
        return rhs < lhs;
    }
    bool operator>=(const DatasetPartitionId &lhs, const DatasetPartitionId &rhs) noexcept {
        return !(lhs < rhs);
    }
    bool operator<(const DatasetPartitionId &lhs, const DatasetPartitionId &rhs) noexcept {
        return lhs.value_ < rhs.value_;
    }
    bool operator<=(const DatasetPartitionId &lhs, const DatasetPartitionId &rhs) noexcept {
        return !(lhs > rhs);
    }
}






