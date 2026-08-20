//
// Created by hakgu on 8/19/2026.
//

#include "core/datasets/llvip_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace qart::core::datasets {

namespace {

using domain::datasets::DatasetLayoutError;
using domain::datasets::DatasetPair;
using domain::datasets::DatasetPairingError;
using domain::datasets::DatasetPartitionId;

constexpr const char* kDatasetId =
    "llvip";

constexpr const char* kVisibleDirectoryName =
    "visible";

constexpr const char* kThermalDirectoryName =
    "infrared";

constexpr const char* kSupportedExtension =
    ".jpg";

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
        const std::filesystem::path& entry
        : entries
    ) {
        if (
            !std::filesystem::is_regular_file(
                entry
            )
        ) {
            continue;
        }

        if (
            lowercaseAscii(
                entry.extension().string()
            )
            != kSupportedExtension
        ) {
            continue;
        }

        const std::string sourceStem =
            entry.stem().string();

        const std::string filename =
            entry.filename().string();

        const auto existingStem =
            stemToFilename.find(
                sourceStem
            );

        if (
            existingStem
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
                + existingStem->second
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
            filename,
            entry
        );
    }

    return indexed;
}

} // namespace

std::string
LLVIPAdapter::datasetId() const
{
    return kDatasetId;
}

std::vector<DatasetPartitionId>
LLVIPAdapter::supportedPartitions() const
{
    return {
        DatasetPartitionId("train"),
        DatasetPartitionId("test")
    };
}

std::filesystem::path
LLVIPAdapter::datasetDirectory(
    const std::filesystem::path& datasetRoot
)
{
    return datasetRoot
        / "raw"
        / "llvip";
}

std::filesystem::path
LLVIPAdapter::partitionDirectory(
    const std::filesystem::path& datasetRoot,
    const std::string& modalityDirectoryName,
    const DatasetPartitionId& partitionId
)
{
    return datasetDirectory(
        datasetRoot
    )
        / modalityDirectoryName
        / partitionId.value();
}

void LLVIPAdapter::validateLayout(
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

    const std::filesystem::path
        llvipDirectory =
            datasetDirectory(
                datasetRoot
            );

    if (
        !std::filesystem::is_directory(
            llvipDirectory
        )
    ) {
        throw DatasetLayoutError(
            "LLVIP dataset directory does not exist: "
            + llvipDirectory.string()
        );
    }

    std::vector<std::filesystem::path>
        missingDirectories;

    for (
        const std::string modality
        : {
            std::string(
                kVisibleDirectoryName
            ),
            std::string(
                kThermalDirectoryName
            )
        }
    ) {
        for (
            const DatasetPartitionId&
                partition
            : supportedPartitions()
        ) {
            const auto requiredDirectory =
                partitionDirectory(
                    datasetRoot,
                    modality,
                    partition
                );

            if (
                !std::filesystem::is_directory(
                    requiredDirectory
                )
            ) {
                missingDirectories.push_back(
                    requiredDirectory
                );
            }
        }
    }

    if (!missingDirectories.empty()) {
        std::string message =
            "LLVIP is missing required directories:";

        for (
            const std::filesystem::path& path
            : missingDirectories
        ) {
            message +=
                "\n- "
                + path.string();
        }

        throw DatasetLayoutError(
            message
        );
    }
}

std::string
LLVIPAdapter::toManifestRelativePath(
    const std::filesystem::path& physicalPath,
    const std::filesystem::path& datasetRoot
)
{
    const std::filesystem::path
        relativePath =
            physicalPath.lexically_relative(
                datasetRoot
            );

    if (
        relativePath.empty()
        || relativePath.is_absolute()
    ) {
        throw DatasetPairingError(
            "LLVIP candidate path could not be made "
            "relative to the shared dataset root."
        );
    }

    return relativePath.generic_string();
}

std::vector<DatasetPair>
LLVIPAdapter::discoverPairs(
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
            "LLVIP does not support partition '"
            + partitionId.value()
            + "'."
        );
    }

    validateLayout(
        datasetRoot
    );

    const auto visibleDirectory =
        partitionDirectory(
            datasetRoot,
            kVisibleDirectoryName,
            partitionId
        );

    const auto thermalDirectory =
        partitionDirectory(
            datasetRoot,
            kThermalDirectoryName,
            partitionId
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
        candidateFilenames;

    for (
        const auto& entry
        : visibleFiles
    ) {
        candidateFilenames.insert(
            entry.first
        );
    }

    for (
        const auto& entry
        : thermalFiles
    ) {
        candidateFilenames.insert(
            entry.first
        );
    }

    std::vector<DatasetPair>
        discoveredPairs;

    discoveredPairs.reserve(
        candidateFilenames.size()
    );

    for (
        const std::string& filename
        : candidateFilenames
    ) {
        const std::string sourceStem =
            std::filesystem::path(
                filename
            )
                .stem()
                .string();

        const auto visibleIterator =
            visibleFiles.find(
                filename
            );

        const auto thermalIterator =
            thermalFiles.find(
                filename
            );

        const std::filesystem::path
            visiblePath =
                visibleIterator
                    != visibleFiles.end()
                ? visibleIterator->second
                : visibleDirectory
                    / filename;

        const std::filesystem::path
            thermalPath =
                thermalIterator
                    != thermalFiles.end()
                ? thermalIterator->second
                : thermalDirectory
                    / filename;

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