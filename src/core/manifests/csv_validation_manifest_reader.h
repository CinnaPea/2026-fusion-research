//
// Created by hakgu on 8/18/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_CSV_VALIDATION_MANIFEST_READER_H
#define VISUAL_THERMAL_CONCEPT_CSV_VALIDATION_MANIFEST_READER_H

#pragma once

#include "core/manifests/validation_manifest.h"

#include <filesystem>

namespace qart::core::manifests {

    class CsvValidationManifestReader final
    {
    public:
        [[nodiscard]]
        ValidationManifest read(
            const std::filesystem::path& inputPath
        ) const;
    };

} // namespace qart::core::manifests

#endif //VISUAL_THERMAL_CONCEPT_CSV_VALIDATION_MANIFEST_READER_H