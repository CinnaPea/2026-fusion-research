//
// Created by hakgu on 10/9/2026.
//

#include "core/datasets/tno_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace qart::core::datasets {

namespace {

using domain::datasets::DatasetLayoutError;
using domain::datasets::DatasetPair;
using domain::datasets::DatasetPairingError;
using domain::datasets::DatasetPartitionId;

constexpr const char* kDatasetId = "tno";
constexpr const char* kVisibleDirectoryName = "vi";
constexpr const char* kThermalDirectoryName = "ir";
constexpr const char* kSupportedExtension = ".png";

std::string lowercaseAscii(
    std::string value
)
{
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(character)
            )
        );
    }

    return value;
}

std::map<std::string, std::filesystem::path>
indexSupportedFiles(
    const std::filesystem::path& directory,
    const std::string& modalityName
)
{
    std::vector<std::filesystem::path> entries;

    for (
        const auto& entry
        : std::filesystem::directory_iterator(directory)
    ) {
        entries.push_back(entry.path());
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](
            const std::filesystem::path& left,
            const std::filesystem::path& right
        ) {
            return left.filename().string()
                < right.filename().string();
        }
    );

    std::map<std::string, std::filesystem::path> indexed;

    for (const auto& path : entries) {
        if (!std::filesystem::is_regular_file(path)) {
            continue;
        }

        if (
            lowercaseAscii(path.extension().string())
            != kSupportedExtension
        ) {
            continue;
        }

        const std::string sourceStem =
            path.stem().string();

        if (indexed.find(sourceStem) != indexed.end()) {
            throw DatasetPairingError(
                "Duplicate "
                + modalityName
                + " source stem '"
                + sourceStem
                + "' found in "
                + directory.string()
                + "."
            );
        }

        indexed.emplace(
            sourceStem,
            path
        );
    }

    return indexed;
}

} // namespace


std::string TNOAdapter::datasetId() const
{
    return kDatasetId;
}


std::vector<DatasetPartitionId>
TNOAdapter::supportedPartitions() const
{
    return {
        DatasetPartitionId("all")
    };
}


std::filesystem::path
TNOAdapter::datasetDirectory(
    const std::filesystem::path& datasetRoot
)
{
    return datasetRoot
        / "raw"
        / "tno";
}


std::filesystem::path
TNOAdapter::modalityDirectory(
    const std::filesystem::path& datasetRoot,
    const std::string& modalityDirectoryName
)
{
    return datasetDirectory(datasetRoot)
        / modalityDirectoryName;
}


void TNOAdapter::validateLayout(
    const std::filesystem::path& datasetRoot
) const
{
    if (!std::filesystem::exists(datasetRoot)) {
        throw DatasetLayoutError(
            "Shared dataset root does not exist: "
            + datasetRoot.string()
        );
    }

    if (!std::filesystem::is_directory(datasetRoot)) {
        throw DatasetLayoutError(
            "Shared dataset root is not a directory: "
            + datasetRoot.string()
        );
    }

    const auto m3fdDirectory =
        datasetDirectory(datasetRoot);

    if (!std::filesystem::is_directory(m3fdDirectory)) {
        throw DatasetLayoutError(
            "M3FD dataset directory does not exist: "
            + m3fdDirectory.string()
        );
    }

    const std::vector<std::filesystem::path>
        requiredDirectories = {
            modalityDirectory(
                datasetRoot,
                kVisibleDirectoryName
            ),
            modalityDirectory(
                datasetRoot,
                kThermalDirectoryName
            )
        };

    std::vector<std::filesystem::path>
        missingDirectories;

    for (const auto& directory : requiredDirectories) {
        if (!std::filesystem::is_directory(directory)) {
            missingDirectories.push_back(directory);
        }
    }

    if (!missingDirectories.empty()) {
        std::string message =
            "M3FD is missing required directories:";

        for (const auto& directory : missingDirectories) {
            message +=
                "\n- "
                + directory.string();
        }

        throw DatasetLayoutError(message);
    }
}


std::string TNOAdapter::toManifestRelativePath(
    const std::filesystem::path& physicalPath,
    const std::filesystem::path& datasetRoot
)
{
    const auto relativePath =
        physicalPath.lexically_relative(datasetRoot);

    if (
        relativePath.empty()
        || relativePath.is_absolute()
    ) {
        throw DatasetPairingError(
            "M3FD candidate path could not be made "
            "relative to the shared dataset root."
        );
    }

    for (const auto& component : relativePath) {
        if (component == "..") {
            throw DatasetPairingError(
                "M3FD candidate path escapes "
                "the shared dataset root."
            );
        }
    }

    return relativePath.generic_string();
}


std::vector<DatasetPair>
TNOAdapter::discoverPairs(
    const std::filesystem::path& datasetRoot,
    const DatasetPartitionId& partitionId
) const
{
    if (!supportsPartition(partitionId)) {
        throw DatasetPairingError(
            "M3FD does not support partition '"
            + partitionId.value()
            + "'."
        );
    }

    validateLayout(datasetRoot);

    const auto visibleDirectory =
        modalityDirectory(
            datasetRoot,
            kVisibleDirectoryName
        );

    const auto thermalDirectory =
        modalityDirectory(
            datasetRoot,
            kThermalDirectoryName
        );

    const auto visibleFiles =
        indexSupportedFiles(
            visibleDirectory,
            "visible"
        );

    const auto thermalFiles =
        indexSupportedFiles(
            thermalDirectory,
            "thermal"
        );

    std::set<std::string> candidateStems;

    for (const auto& entry : visibleFiles) {
        candidateStems.insert(entry.first);
    }

    for (const auto& entry : thermalFiles) {
        candidateStems.insert(entry.first);
    }

    std::vector<DatasetPair> discoveredPairs;
    discoveredPairs.reserve(candidateStems.size());

    for (const std::string& sourceStem : candidateStems) {
        const std::string expectedFilename =
            sourceStem
            + kSupportedExtension;

        const auto visibleIterator =
            visibleFiles.find(sourceStem);

        const auto thermalIterator =
            thermalFiles.find(sourceStem);

        const std::filesystem::path visiblePath =
            visibleIterator != visibleFiles.end()
                ? visibleIterator->second
                : visibleDirectory / expectedFilename;

        const std::filesystem::path thermalPath =
            thermalIterator != thermalFiles.end()
                ? thermalIterator->second
                : thermalDirectory / expectedFilename;

        discoveredPairs.emplace_back(
            datasetId()
                + "_"
                + partitionId.value()
                + "_"
                + sourceStem,
            datasetId(),
            partitionId,
            sourceStem,
            toManifestRelativePath(
                visiblePath,
                datasetRoot
            ),
            toManifestRelativePath(
                thermalPath,
                datasetRoot
            )
        );
    }

    return discoveredPairs;
}

} // namespace qart::core::datasets