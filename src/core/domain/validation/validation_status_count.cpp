//
// Created by hakgu on 8/16/2026.
//

#include "validation_status_count.h"
#include <stdexcept>

namespace qart::core::domain::validation {
    ValidationStatusCount::ValidationStatusCount(PairValidationStatus sts, int cnt) : m_sts(sts), m_count(cnt) {
        if (m_count < 0) {
            throw std::invalid_argument("Invalid validation status count");
        }
    }

    PairValidationStatus ValidationStatusCount::status() const noexcept {
        return m_sts;
    }
    int ValidationStatusCount::count() const noexcept {
        return m_count;
    }
    bool operator==(const ValidationStatusCount& lhs, const ValidationStatusCount& rhs) noexcept {
        return lhs.m_sts == rhs.m_sts && lhs.m_count == rhs.m_count;
    }
    bool operator!=(const ValidationStatusCount& lhs, const ValidationStatusCount& rhs) noexcept {
        return !(lhs == rhs);
    }
}