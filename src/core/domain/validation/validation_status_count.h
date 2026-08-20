//
// Created by hakgu on 8/16/2026.
//

#pragma once

#include "pair_validation_status.h"

#ifndef VISUAL_THERMAL_CONCEPT_VALIDATION_STATUS_COUNT_H
#define VISUAL_THERMAL_CONCEPT_VALIDATION_STATUS_COUNT_H

namespace qart::core::domain::validation {
    class ValidationStatusCount final {
    public:
        ValidationStatusCount(PairValidationStatus sts, int cnt);

        [[nodiscard]]
        PairValidationStatus status() const noexcept;

        [[nodiscard]]
        int count() const noexcept;

        friend bool operator==(const ValidationStatusCount& lhs, const ValidationStatusCount& rhs) noexcept;
        friend bool operator!=(const ValidationStatusCount& lhs, const ValidationStatusCount& rhs) noexcept;
    private:
        PairValidationStatus m_sts;
        int m_count;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_VALIDATION_STATUS_COUNT_H