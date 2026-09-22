//
// Created by hakgu on 8/17/2026.
//

#pragma once
#include "dataset_configuration.h"
#include<filesystem>

#ifndef VISUAL_THERMAL_CONCEPT_JSON_DATASET_CONFIGURATION_LOADER_H
#define VISUAL_THERMAL_CONCEPT_JSON_DATASET_CONFIGURATION_LOADER_H

namespace qart::core::configuration {
    class JsonDatasetConfigurationLoader final {
    public:
        [[nodiscard]]
        static DatasetConfiguration load(const std::filesystem::path& configPath);
    };
}

#endif //VISUAL_THERMAL_CONCEPT_JSON_DATASET_CONFIGURATION_LOADER_H