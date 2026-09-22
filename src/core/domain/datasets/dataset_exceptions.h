//
// Created by hakgu on 8/14/2026.
//
#pragma once

#include<stdexcept>

#ifndef VISUAL_THERMAL_CONCEPT_DATASET_EXCEPTIONS_H
#define VISUAL_THERMAL_CONCEPT_DATASET_EXCEPTIONS_H

namespace qart::core::domain::datasets {
    class DatasetError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class DatasetConfigurationError final : public DatasetError {
    public:
        using DatasetError::DatasetError;
    };

    class DatasetLayoutError final : public DatasetError
    {
    public:
        using DatasetError::DatasetError;
    };

    class DatasetPairingError final : public DatasetError
    {
    public:
        using DatasetError::DatasetError;
    };

    class DatasetManifestError final : public DatasetError
    {
    public:
        using DatasetError::DatasetError;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_DATASET_EXCEPTIONS_H