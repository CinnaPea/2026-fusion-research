#include "application/application_controller.h"

#include "core/datasets/llvip_adapter.h"
#include "core/datasets/msrs_adapter.h"
#include "core/datasets/roadscene_adapter.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/manifests/csv_validation_manifest_reader.h"
#include "core/manifests/validation_manifest.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace qart::application {

namespace {

using core::domain::datasets::DatasetPair;
using core::domain::datasets::DatasetPartitionId;
using core::domain::validation::PairValidationPolicy;
using core::domain::validation::PairValidationResult;

std::vector<DatasetPair> discoverWithAdapter(
    const core::datasets::DatasetAdapter& adapter,
    const std::filesystem::path& datasetRoot
)
{
    adapter.validateLayout(datasetRoot);

    std::vector<DatasetPair> discoveredPairs;
    for (const auto& partition : adapter.supportedPartitions()) {
        auto pairs = adapter.discoverPairs(datasetRoot, partition);
        discoveredPairs.insert(
            discoveredPairs.end(),
            pairs.begin(),
            pairs.end()
        );
    }

    return discoveredPairs;
}

} // namespace

bool ApplicationController::loadDataset(
    const std::filesystem::path& inputPath,
    const std::string& requestedDatasetId
)
{
    clearDatasetSession();
    datasetRoot_ = resolveDatasetRoot(inputPath);

    std::vector<DatasetPair> discoveredPairs;
    std::string detectedDatasetId;

    if (requestedDatasetId.empty() || requestedDatasetId == "llvip") {
        try {
            core::datasets::LLVIPAdapter adapter;
            discoveredPairs = discoverWithAdapter(adapter, datasetRoot_);
            detectedDatasetId = adapter.datasetId();
        } catch (...) {
            discoveredPairs.clear();
            detectedDatasetId.clear();
        }
    }

    if (
        discoveredPairs.empty()
        && (requestedDatasetId.empty() || requestedDatasetId == "msrs")
    ) {
        try {
            core::datasets::MSRSAdapter adapter;
            discoveredPairs = discoverWithAdapter(adapter, datasetRoot_);
            detectedDatasetId = adapter.datasetId();
        } catch (...) {
            discoveredPairs.clear();
            detectedDatasetId.clear();
        }
    }

    if (
        discoveredPairs.empty()
        && (requestedDatasetId.empty() || requestedDatasetId == "roadscene")
    ) {
        try {
            core::datasets::RoadSceneAdapter adapter;
            discoveredPairs = discoverWithAdapter(adapter, datasetRoot_);
            detectedDatasetId = adapter.datasetId();
        } catch (...) {
            discoveredPairs.clear();
            detectedDatasetId.clear();
        }
    }

    if (discoveredPairs.empty()) {
        clearDatasetSession();
        datasetRoot_ = resolveDatasetRoot(inputPath);
        return false;
    }

    std::string manifestRelativePath;
    std::vector<DatasetPartitionId> configuredPartitions;

    if (detectedDatasetId == "llvip") {
        manifestRelativePath =
            "manifests/llvip/llvip_validation_schema_2_0.csv";
        configuredPartitions = {
            DatasetPartitionId("train"),
            DatasetPartitionId("test")
        };
    } else if (detectedDatasetId == "msrs") {
        manifestRelativePath =
            "manifests/msrs/msrs_validation_schema_2_0.csv";
        configuredPartitions = {
            DatasetPartitionId("train"),
            DatasetPartitionId("test")
        };
    } else if (detectedDatasetId == "roadscene") {
        manifestRelativePath =
            "manifests/roadscene/roadscene_validation_schema_2_0.csv";
        configuredPartitions = {
            DatasetPartitionId("all")
        };
    } else {
        throw std::runtime_error(
            "Unsupported detected dataset ID: " + detectedDatasetId
        );
    }

    currentDatasetId_ = detectedDatasetId;
    discoveredPairs_ = std::move(discoveredPairs);

    datasetConfiguration_.emplace(
        "2.0",
        currentDatasetId_,
        datasetRoot_,
        manifestRelativePath,
        configuredPartitions,
        validationPolicyForDataset(currentDatasetId_)
    );

    loadCanonicalValidationManifest();
    return true;
}

void ApplicationController::clearDatasetSession() noexcept
{
    currentDatasetId_.clear();
    discoveredPairs_.clear();
    datasetConfiguration_.reset();
    manifestResults_.clear();
    canonicalManifestError_.clear();
}

bool ApplicationController::hasDatasetSession() const noexcept
{
    return !currentDatasetId_.empty() && !discoveredPairs_.empty();
}

const std::filesystem::path&
ApplicationController::datasetRoot() const noexcept
{
    return datasetRoot_;
}

const std::string&
ApplicationController::currentDatasetId() const noexcept
{
    return currentDatasetId_;
}

const std::vector<core::domain::datasets::DatasetPair>&
ApplicationController::discoveredPairs() const noexcept
{
    return discoveredPairs_;
}

std::filesystem::path ApplicationController::resolveDatasetRoot(
    const std::filesystem::path& inputPath
) const
{
    if (!std::filesystem::exists(inputPath)) {
        return inputPath;
    }

    std::filesystem::path current = inputPath;
    if (std::filesystem::is_regular_file(current)) {
        current = current.parent_path();
    }

    std::error_code error;
    current = std::filesystem::canonical(current, error);
    if (error) {
        current = inputPath;
        if (std::filesystem::is_regular_file(current)) {
            current = current.parent_path();
        }
    }

    if (
        std::filesystem::exists(current / "raw")
        && std::filesystem::is_directory(current / "raw")
    ) {
        return current;
    }

    if (current.filename() == "raw" && current.has_parent_path()) {
        return current.parent_path();
    }

    std::string folderName = current.filename().string();
    std::transform(
        folderName.begin(),
        folderName.end(),
        folderName.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );

    if (
        folderName == "llvip"
        || folderName == "msrs"
        || folderName == "roadscene"
    ) {
        if (current.has_parent_path()) {
            const std::filesystem::path parent = current.parent_path();
            if (parent.filename() == "raw" && parent.has_parent_path()) {
                return parent.parent_path();
            }
            return parent;
        }
    }

    std::filesystem::path search = current;
    for (int depth = 0; depth < 5; ++depth) {
        if (
            std::filesystem::exists(search / "raw")
            && std::filesystem::is_directory(search / "raw")
        ) {
            return search;
        }

        if (!search.has_parent_path() || search == search.parent_path()) {
            break;
        }

        search = search.parent_path();
    }

    return current;
}

PairValidationPolicy ApplicationController::validationPolicyForDataset(
    const std::string& datasetId
) const
{
    const std::vector<std::string> supportedExtensions = {
        ".jpg",
        ".jpeg",
        ".png",
        ".bmp"
    };

    if (datasetId == "llvip") {
        return PairValidationPolicy(
            supportedExtensions,
            supportedExtensions,
            core::domain::imaging::ImageDimensions(1280, 1024),
            core::domain::imaging::ImageDimensions(1280, 1024)
        );
    }

    if (datasetId == "msrs") {
        return PairValidationPolicy(
            supportedExtensions,
            supportedExtensions,
            core::domain::imaging::ImageDimensions(640, 480),
            core::domain::imaging::ImageDimensions(640, 480)
        );
    }

    return PairValidationPolicy(
        supportedExtensions,
        supportedExtensions,
        std::nullopt,
        std::nullopt
    );
}

std::vector<PairValidationResult>
ApplicationController::previewCandidates(
    const std::vector<DatasetPair>& pairs
) const
{
    if (!canonicalManifestError_.empty()) {
        throw std::runtime_error(canonicalManifestError_);
    }

    if (!datasetConfiguration_.has_value() || manifestResults_.empty()) {
        throw std::runtime_error(
            "Mode B/C requires a complete canonical Schema 2.0 manifest."
        );
    }

    std::vector<PairValidationResult> results;
    results.reserve(pairs.size());

    for (const auto& pair : pairs) {
        const auto manifested = manifestResults_.find(pair.pairId());
        if (manifested == manifestResults_.end()) {
            throw std::runtime_error(
                "Canonical manifest is missing filtered pair ID: "
                + pair.pairId()
            );
        }
        results.push_back(manifested->second);
    }

    return results;
}

void ApplicationController::loadCanonicalValidationManifest()
{
    manifestResults_.clear();
    canonicalManifestError_.clear();

    if (!datasetConfiguration_.has_value()) {
        canonicalManifestError_ =
            "Canonical dataset configuration is unavailable.";
        return;
    }

    const std::filesystem::path manifestPath =
        datasetConfiguration_->resolveManifestPath();

    if (!std::filesystem::is_regular_file(manifestPath)) {
        canonicalManifestError_ =
            "Canonical manifest does not exist at configured path: "
            + manifestPath.string();
        return;
    }

    try {
        core::manifests::CsvValidationManifestReader reader;
        const core::manifests::ValidationManifest manifest =
            reader.read(manifestPath);
        const auto results = manifest.toResults();

        std::unordered_map<std::string, const DatasetPair*> discoveredById;
        discoveredById.reserve(discoveredPairs_.size());
        for (const auto& pair : discoveredPairs_) {
            discoveredById.emplace(pair.pairId(), &pair);
        }

        std::unordered_set<std::string> manifestIds;
        manifestIds.reserve(results.size());

        for (const auto& result : results) {
            if (result.pair().datasetId() != datasetConfiguration_->datasetId()) {
                throw std::runtime_error(
                    "Canonical manifest contains a pair from another dataset: "
                    + result.pair().pairId()
                );
            }

            const auto discovered =
                discoveredById.find(result.pair().pairId());
            if (
                discovered == discoveredById.end()
                || *discovered->second != result.pair()
            ) {
                throw std::runtime_error(
                    "Canonical manifest pair metadata is stale or non-canonical: "
                    + result.pair().pairId()
                );
            }

            manifestIds.insert(result.pair().pairId());
        }

        if (manifestIds.size() != discoveredById.size()) {
            throw std::runtime_error(
                "Canonical manifest is incomplete or stale for the current dataset "
                "(manifest pairs=" + std::to_string(manifestIds.size())
                + ", discovered pairs="
                + std::to_string(discoveredById.size()) + ")."
            );
        }

        for (auto result : results) {
            manifestResults_.emplace(
                result.pair().pairId(),
                std::move(result)
            );
        }
    } catch (const std::exception& exception) {
        manifestResults_.clear();
        canonicalManifestError_ =
            "Cannot load complete canonical manifest '"
            + manifestPath.string() + "': " + exception.what();
    }
}

} // namespace qart::application
