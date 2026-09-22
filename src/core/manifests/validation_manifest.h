//
// Created by hakgu on 8/18/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_VALIDATION_MANIFEST_H
#define VISUAL_THERMAL_CONCEPT_VALIDATION_MANIFEST_H

#pragma once

#include "core/domain/validation/batch_validation_report.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/manifests/validation_manifest_row.h"

#include <string>
#include <vector>

namespace qart::core::manifests {

    class ValidationManifest final
    {
    public:
        ValidationManifest(
            std::string schemaVersion,
            std::vector<ValidationManifestRow> rows
        );

        [[nodiscard]]
        static ValidationManifest fromReport(
            const domain::validation::BatchValidationReport& report
        );

        [[nodiscard]]
        std::vector<
            domain::validation::PairValidationResult
        > toResults() const;

        [[nodiscard]]
        const std::string&
        schemaVersion() const noexcept;

        [[nodiscard]]
        const std::vector<ValidationManifestRow>&
        rows() const noexcept;

        friend bool operator==(
            const ValidationManifest& left,
            const ValidationManifest& right
        ) noexcept;

        friend bool operator!=(
            const ValidationManifest& left,
            const ValidationManifest& right
        ) noexcept;

    private:
        void validate() const;

        std::string schemaVersion_;
        std::vector<ValidationManifestRow> rows_;
    };

} // namespace qart::core::manifests

#endif //VISUAL_THERMAL_CONCEPT_VALIDATION_MANIFEST_H