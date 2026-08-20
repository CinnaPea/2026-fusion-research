//
// Created by hakgu on 8/20/2026.
//

#include "core/datasets/roadscene_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace qart::core::datasets {

namespace {

using domain::datasets::DatasetLayoutError;
using domain::datasets::DatasetPair;
using domain::datasets::DatasetPairingError;
using domain::datasets::DatasetPartitionId;

constexpr const char* kDatasetId =
    "roadscene";

constexpr const char* kVisibleDirectoryName =
    "crop_LR_visible";

constexpr const char* kThermalDirectoryName =
    "cropinfrared";

constexpr const char* kSupportedExtension =
    ".jpg";

const std::regex kSourceStemPattern(
    R"(^FLIR_(?:video_)?[0-9]+$)"
);

std::string lowercaseAscii(
    std::string value
)
{
    for (char& character : value) {
        character =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        character
                    )
                )
            );
    }

    return value;
}

std::map<
    std::string,
    std::filesystem::path
> indexSupportedFiles(
    const std::filesystem::path& directory,
    const std::string& modalityName
)
{
    std::vector<std::filesystem::path>
        entries;

    for (
        const auto& entry
        : std::filesystem::directory_iterator(
              directory
          )
    ) {
        entries.push_back(
            entry.path()
        );
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

    std::map<
        std::string,
        std::filesystem::path
    > indexed;

    std::map<
        std::string,
        std::string
    > stemToFilename;

    for (
        const std::filesystem::path& path
        : entries
    ) {
        if (
            !std::filesystem::is_regular_file(
                path
            )
        ) {
            continue;
        }

        if (
            lowercaseAscii(
                path.extension().string()
            )
            != kSupportedExtension
        ) {
            continue;
        }

        const std::string sourceStem =
            path.stem().string();

        const std::string filename =
            path.filename().string();

        const auto existing =
            stemToFilename.find(
                sourceStem
            );

        if (
            existing
            != stemToFilename.end()
        ) {
            throw DatasetPairingError(
                "Duplicate "
                + modalityName
                + " source stem '"
                + sourceStem
                + "' found in "
                + directory.string()
                + ": '"
                + existing->second
                + "' and '"
                + filename
                + "'."
            );
        }

        stemToFilename.emplace(
            sourceStem,
            filename
        );

        indexed.emplace(
            sourceStem,
            path
        );
    }

    return indexed;
}

} // namespace

std::string
RoadSceneAdapter::datasetId() const
{
    return kDatasetId;
}

std::vector<DatasetPartitionId>
RoadSceneAdapter::supportedPartitions() const
{
    return {
        DatasetPartitionId("all")
    };
}

std::filesystem::path
RoadSceneAdapter::datasetDirectory(
    const std::filesystem::path& datasetRoot
)
{
    return datasetRoot
        / "raw"
        / "roadscene";
}

std::filesystem::path
RoadSceneAdapter::modalityDirectory(
    const std::filesystem::path& datasetRoot,
    const std::string& modalityDirectoryName
)
{
    return datasetDirectory(
        datasetRoot
    )
        / modalityDirectoryName;
}

void RoadSceneAdapter::validateSourceStem(
    const std::string& sourceStem
)
{
    if (
        !std::regex_match(
            sourceStem,
            kSourceStemPattern
        )
    ) {
        throw DatasetPairingError(
            "Invalid RoadScene source stem '"
            + sourceStem
            + "'. Expected FLIR_<digits> or "
              "FLIR_video_<digits>."
        );
    }
}

void RoadSceneAdapter::validateLayout(
    const std::filesystem::path& datasetRoot
) const
{
    if (
        !std::filesystem::exists(
            datasetRoot
        )
    ) {
        throw DatasetLayoutError(
            "Shared dataset root does not exist: "
            + datasetRoot.string()
        );
    }

    if (
        !std::filesystem::is_directory(
            datasetRoot
        )
    ) {
        throw DatasetLayoutError(
            "Shared dataset root is not a directory: "
            + datasetRoot.string()
        );
    }

    const auto roadsceneDirectory =
        datasetDirectory(
            datasetRoot
        );

    if (
        !std::filesystem::is_directory(
            roadsceneDirectory
        )
    ) {
        throw DatasetLayoutError(
            "RoadScene dataset directory does not exist: "
            + roadsceneDirectory.string()
        );
    }

    const std::vector<
        std::filesystem::path
    > requiredDirectories = {
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

    for (
        const auto& directory
        : requiredDirectories
    ) {
        if (
            !std::filesystem::is_directory(
                directory
            )
        ) {
            missingDirectories.push_back(
                directory
            );
        }
    }

    if (!missingDirectories.empty()) {
        std::string message =
            "RoadScene is missing canonical fusion directories:";

        for (
            const auto& directory
            : missingDirectories
        ) {
            message +=
                "\n- "
                + directory.string();
        }

        throw DatasetLayoutError(
            message
        );
    }
}

std::string
RoadSceneAdapter::toManifestRelativePath(
    const std::filesystem::path& physicalPath,
    const std::filesystem::path& datasetRoot
)
{
    const auto relativePath =
        physicalPath.lexically_relative(
            datasetRoot
        );

    if (
        relativePath.empty()
        || relativePath.is_absolute()
    ) {
        throw DatasetPairingError(
            "RoadScene candidate path could not be made "
            "relative to the shared dataset root."
        );
    }

    for (
        const auto& component
        : relativePath
    ) {
        if (component == "..") {
            throw DatasetPairingError(
                "RoadScene candidate path escapes "
                "the shared dataset root."
            );
        }
    }

    return relativePath.generic_string();
}

std::vector<DatasetPair>
RoadSceneAdapter::discoverPairs(
    const std::filesystem::path& datasetRoot,
    const DatasetPartitionId& partitionId
) const
{
    if (
        !supportsPartition(
            partitionId
        )
    ) {
        throw DatasetPairingError(
            "RoadScene does not support partition '"
            + partitionId.value()
            + "'."
        );
    }

    validateLayout(
        datasetRoot
    );

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

    std::set<std::string>
        candidateStems;

    for (
        const auto& entry
        : visibleFiles
    ) {
        candidateStems.insert(
            entry.first
        );
    }

    for (
        const auto& entry
        : thermalFiles
    ) {
        candidateStems.insert(
            entry.first
        );
    }

    std::vector<DatasetPair>
        discoveredPairs;

    discoveredPairs.reserve(
        candidateStems.size()
    );

    for (
        const std::string& sourceStem
        : candidateStems
    ) {
        validateSourceStem(
            sourceStem
        );

        const std::string expectedFilename =
            sourceStem
            + kSupportedExtension;

        const auto visibleIterator =
            visibleFiles.find(
                sourceStem
            );

        const auto thermalIterator =
            thermalFiles.find(
                sourceStem
            );

        const std::filesystem::path visiblePath =
            visibleIterator
                != visibleFiles.end()
            ? visibleIterator->second
            : visibleDirectory
                / expectedFilename;

        const std::filesystem::path thermalPath =
            thermalIterator
                != thermalFiles.end()
            ? thermalIterator->second
            : thermalDirectory
                / expectedFilename;

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