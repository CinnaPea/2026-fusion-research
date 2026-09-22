//
// Created by hakgu on 8/19/2026.
//

#pragma once
#include "validation_manifest.h"
#include <filesystem>

#ifndef VISUAL_THERMAL_CONCEPT_CSV_VALIDATION_MANIFEST_WRIT_H
#define VISUAL_THERMAL_CONCEPT_CSV_VALIDATION_MANIFEST_WRIT_H

namespace qart::core::manifests {
    class CsvValidationManifestWriter final {
    public:
        [[nodiscard]]
        std::filesystem::path write(const ValidationManifest& mnf, const std::filesystem::path& outputPath, bool overwrite = false) const;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_CSV_VALIDATION_MANIFEST_WRIT_H