//
// Created by hakgu on 8/18/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_JSON_EXPERIMENT_VIEW_LOADER_H
#define VISUAL_THERMAL_CONCEPT_JSON_EXPERIMENT_VIEW_LOADER_H

#pragma once

#include "core/domain/datasets/experiment_view.h"

#include <filesystem>
#include <string_view>

namespace qart::core::configuration {

    inline constexpr std::string_view
        kExperimentViewSchemaVersion = "1.0";

    class JsonExperimentViewLoader final
    {
    public:
        [[nodiscard]]
        domain::datasets::ExperimentView load(
            const std::filesystem::path& configPath
        ) const;
    };

} // namespace qart::core::configuration

#endif //VISUAL_THERMAL_CONCEPT_JSON_EXPERIMENT_VIEW_LOADER_H