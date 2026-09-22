#ifndef VISUAL_THERMAL_CONCEPT_APPLICATION_CONTROLLER_H
#define VISUAL_THERMAL_CONCEPT_APPLICATION_CONTROLLER_H

#pragma once

#include "core/configuration/dataset_configuration.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/visualization/preview_selection.h"

#include <functional>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>

namespace qart::application {

struct ValidationExportItem
{
    core::domain::datasets::DatasetPair pair;

    std::optional<
        core::domain::validation::PairValidationResult
    > validationResult;
};

struct ValidationExportResult
{
    std::filesystem::path writtenPath;
    std::size_t rowCount;
};

enum class SingleImageLookupStatus
{
    Resolved,
    UnsupportedDataset,
    DatasetLoadFailed,
    NonCanonicalInput
};

struct SingleImageLookupResult
{
    SingleImageLookupStatus status;
    std::filesystem::path datasetRoot;
    std::string datasetId;
    std::optional<
        core::domain::datasets::DatasetPair
    > pair;
    bool datasetSessionChanged;
};

class ApplicationController final
{
public:
    ApplicationController() = default;

    [[nodiscard]]
    std::filesystem::path
    defaultValidationReportPath() const;

    [[nodiscard]]
    ValidationExportResult exportValidationReport(
        const std::vector<ValidationExportItem>& items,
        const std::filesystem::path& outputPath
    );

    [[nodiscard]]
    std::size_t validateAllPairs(
        const std::function<
            void(std::size_t completed, std::size_t total)
        >& progressCallback = {}
    );

    [[nodiscard]]
    SingleImageLookupResult lookupCanonicalImage(
        const std::filesystem::path& imagePath
    );

    [[nodiscard]]
    core::visualization::PreviewSelection
    selectAutomaticPreview(
        const std::vector<
            core::domain::datasets::DatasetPair
        >& filteredPairs,
        int countPerPartition
    ) const;

    [[nodiscard]]
    core::visualization::PreviewSelection
    selectQueuedPreview(
        const std::vector<
            core::domain::datasets::DatasetPair
        >& filteredPairs,
        int countPerPartition,
        const std::string& selectedPartition
    ) const;

    [[nodiscard]]
    bool addManualSelection(
        const core::domain::datasets::DatasetPair& pair
    );

    void clearManualSelectionQueue() noexcept;

    [[nodiscard]]
    bool isManuallyQueued(
        const std::string& pairId
    ) const noexcept;

    [[nodiscard]]
    std::size_t manualSelectionCount() const noexcept;

    [[nodiscard]]
    core::domain::validation::PairValidationResult getOrValidatePair(const core::domain::datasets::DatasetPair& pair);

    [[nodiscard]]
    bool loadDataset(
        const std::filesystem::path& inputPath,
        const std::string& requestedDatasetId
    );

    [[nodiscard]]
    const std::filesystem::path& datasetRoot() const noexcept;

    [[nodiscard]]
    const std::string& currentDatasetId() const noexcept;

    [[nodiscard]]
    const std::vector<core::domain::datasets::DatasetPair>&
    discoveredPairs() const noexcept;

private:
    void loadCanonicalValidationManifest();

    void clearDatasetSession() noexcept;

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

    std::map<std::string, core::domain::validation::PairValidationResult> validatedResultsCache_;

    std::vector<core::domain::datasets::DatasetPair> manualSelectionQueue_;

    [[nodiscard]]
    static std::filesystem::path resolveDatasetRoot(
        const std::filesystem::path& inputPath
    );

    [[nodiscard]]
    static core::domain::validation::PairValidationPolicy
    validationPolicyForDataset(
        const std::string& datasetId
    );

    [[nodiscard]]
    std::vector<core::domain::validation::PairValidationResult>
    previewCandidates(
        const std::vector<core::domain::datasets::DatasetPair>& pairs
    ) const;
};

} // namespace qart::application

#endif // VISUAL_THERMAL_CONCEPT_APPLICATION_CONTROLLER_H
