#ifndef VISUAL_THERMAL_CONCEPT_APPLICATION_CONTROLLER_H
#define VISUAL_THERMAL_CONCEPT_APPLICATION_CONTROLLER_H

#pragma once

#include "core/configuration/dataset_configuration.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_result.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace qart::application {

class ApplicationController final
{
public:
    ApplicationController() = default;

    [[nodiscard]]
    bool loadDataset(
        const std::filesystem::path& inputPath,
        const std::string& requestedDatasetId
    );

    void clearDatasetSession() noexcept;

    [[nodiscard]]
    bool hasDatasetSession() const noexcept;

    [[nodiscard]]
    const std::filesystem::path& datasetRoot() const noexcept;

    [[nodiscard]]
    const std::string& currentDatasetId() const noexcept;

    [[nodiscard]]
    const std::vector<core::domain::datasets::DatasetPair>&
    discoveredPairs() const noexcept;

    [[nodiscard]]
    std::filesystem::path resolveDatasetRoot(
        const std::filesystem::path& inputPath
    ) const;

    [[nodiscard]]
    core::domain::validation::PairValidationPolicy
    validationPolicyForDataset(
        const std::string& datasetId
    ) const;

    [[nodiscard]]
    std::vector<core::domain::validation::PairValidationResult>
    previewCandidates(
        const std::vector<core::domain::datasets::DatasetPair>& pairs
    ) const;

private:
    void loadCanonicalValidationManifest();

    std::filesystem::path datasetRoot_;
    std::string currentDatasetId_;

    std::vector<core::domain::datasets::DatasetPair>
        discoveredPairs_;

    std::optional<core::configuration::DatasetConfiguration>
        datasetConfiguration_;

    std::map<
        std::string,
        core::domain::validation::PairValidationResult
    > manifestResults_;

    std::string canonicalManifestError_;
};

} // namespace qart::application

#endif // VISUAL_THERMAL_CONCEPT_APPLICATION_CONTROLLER_H
