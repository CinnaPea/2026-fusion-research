//
// Created by hakgu on 8/19/2026.
//

#include "core/datasets/msrs_adapter.h"

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
    "msrs";

constexpr const char* kVisibleDirectoryName =
    "vi";

constexpr const char* kThermalDirectoryName =
    "ir";

constexpr const char* kSupportedExtension =
    ".png";

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

bool isAsciiDigitString(
    const std::string& value
) noexcept
{
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (
            character < '0'
            || character > '9'
        ) {
            return false;
        }
    }

    return true;
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
MSRSAdapter::datasetId() const
{
    return kDatasetId;
}

std::vector<DatasetPartitionId>
MSRSAdapter::supportedPartitions() const
{
    return {
        DatasetPartitionId("train"),
        DatasetPartitionId("test")
    };
}

std::filesystem::path
MSRSAdapter::datasetDirectory(
    const std::filesystem::path& datasetRoot
)
{
    return datasetRoot
        / "raw"
        / "msrs";
}

std::filesystem::path
MSRSAdapter::modalityDirectory(
    const std::filesystem::path& datasetRoot,
    const DatasetPartitionId& partitionId,
    const std::string& modalityDirectoryName
)
{
    return datasetDirectory(
        datasetRoot
    )
        / partitionId.value()
        / modalityDirectoryName;
}

void MSRSAdapter::validateSourceStem(
    const std::string& sourceStem,
    const DatasetPartitionId& partitionId
)
{
    if (sourceStem.empty()) {
        throw DatasetPairingError(
            "Invalid MSRS "
            + partitionId.value()
            + " source stem: stem must not be empty."
        );
    }

    const auto firstNonWhitespace =
        sourceStem.find_first_not_of(
            " \t\n\r\f\v"
        );

    const auto lastNonWhitespace =
        sourceStem.find_last_not_of(
            " \t\n\r\f\v"
        );

    if (
        firstNonWhitespace != 0
        || lastNonWhitespace
            != sourceStem.size() - 1
    ) {
        throw DatasetPairingError(
            "Invalid MSRS "
            + partitionId.value()
            + " source stem '"
            + sourceStem
            + "': stem must not contain surrounding whitespace."
        );
    }

    if (sourceStem.size() < 2) {
        throw DatasetPairingError(
            "Invalid MSRS "
            + partitionId.value()
            + " source stem '"
            + sourceStem
            + "': train/test stems must contain "
              "a numeric prefix and a D or N suffix."
        );
    }

    const std::string numericPrefix =
        sourceStem.substr(
            0,
            sourceStem.size() - 1
        );

    const char conditionSuffix =
        sourceStem.back();

    if (
        !isAsciiDigitString(
            numericPrefix
        )
    ) {
        throw DatasetPairingError(
            "Invalid MSRS "
            + partitionId.value()
            + " source stem '"
            + sourceStem
            + "': train/test stem prefix must "
              "contain digits only."
        );
    }

    if (
        conditionSuffix != 'D'
        && conditionSuffix != 'N'
    ) {
        throw DatasetPairingError(
            "Invalid MSRS "
            + partitionId.value()
            + " source stem '"
            + sourceStem
            + "': train/test stems must end "
              "with D or N."
        );
    }

    bool hasNonZeroDigit = false;

    for (const char digit : numericPrefix) {
        if (digit != '0') {
            hasNonZeroDigit = true;
            break;
        }
    }

    if (!hasNonZeroDigit) {
        throw DatasetPairingError(
            "Invalid MSRS "
            + partitionId.value()
            + " source stem '"
            + sourceStem
            + "': numeric identity must be positive."
        );
    }
}

void MSRSAdapter::validateLayout(
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

    const auto msrsDirectory =
        datasetDirectory(
            datasetRoot
        );

    if (
        !std::filesystem::is_directory(
            msrsDirectory
        )
    ) {
        throw DatasetLayoutError(
            "MSRS dataset directory does not exist: "
            + msrsDirectory.string()
        );
    }

    std::vector<std::filesystem::path>
        missingDirectories;

    for (
        const DatasetPartitionId& partition
        : supportedPartitions()
    ) {
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
            const auto requiredDirectory =
                modalityDirectory(
                    datasetRoot,
                    partition,
                    modality
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
            "MSRS is missing required directories:";

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
MSRSAdapter::toManifestRelativePath(
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
            "MSRS candidate path could not be made "
            "relative to the shared dataset root."
        );
    }

    return relativePath.generic_string();
}

std::vector<DatasetPair>
MSRSAdapter::discoverPairs(
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
            "MSRS does not support partition '"
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
            partitionId,
            kVisibleDirectoryName
        );

    const auto thermalDirectory =
        modalityDirectory(
            datasetRoot,
            partitionId,
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
            sourceStem,
            partitionId
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