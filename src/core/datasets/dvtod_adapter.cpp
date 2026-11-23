#include "core/datasets/dvtod_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace qart::core::datasets {

namespace {

using domain::datasets::DatasetLayoutError;
using domain::datasets::DatasetPair;
using domain::datasets::DatasetPairingError;
using domain::datasets::DatasetPartitionId;

constexpr const char* kDatasetId = "dvtod";
constexpr const char* kPhysicalDatasetDirectory = "dvtdo";
constexpr const char* kVisibleRoot = "vis1";
constexpr const char* kThermalRoot = "ir1";
constexpr const char* kImagesDirectory = "images";
constexpr const char* kSupportedExtension = ".jpg";

std::string lowercaseAscii(std::string value)
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

bool isDecimalStem(const std::string& value)
{
    return !value.empty()
        && std::all_of(
            value.begin(),
            value.end(),
            [](const unsigned char character) {
                return std::isdigit(character) != 0;
            }
        );
}

std::string_view significantDigits(const std::string& value)
{
    const std::size_t firstNonZero = value.find_first_not_of('0');

    if (firstNonZero == std::string::npos) {
        return std::string_view(value).substr(value.size() - 1);
    }

    return std::string_view(value).substr(firstNonZero);
}

bool naturalStemLess(
    const std::string& left,
    const std::string& right
)
{
    const bool leftNumeric = isDecimalStem(left);
    const bool rightNumeric = isDecimalStem(right);

    if (leftNumeric != rightNumeric) {
        return leftNumeric;
    }

    if (!leftNumeric) {
        return left < right;
    }

    const std::string_view leftDigits = significantDigits(left);
    const std::string_view rightDigits = significantDigits(right);

    if (leftDigits.size() != rightDigits.size()) {
        return leftDigits.size() < rightDigits.size();
    }

    if (leftDigits != rightDigits) {
        return leftDigits < rightDigits;
    }

    return left < right;
}

std::map<std::string, std::filesystem::path>
indexSupportedFiles(
    const std::filesystem::path& directory,
    const std::string& modalityName
)
{
    std::map<std::string, std::filesystem::path> indexed;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto& path = entry.path();

        if (lowercaseAscii(path.extension().string()) != kSupportedExtension) {
            continue;
        }

        const std::string sourceStem = path.stem().string();

        if (indexed.find(sourceStem) != indexed.end()) {
            throw DatasetPairingError(
                "Duplicate " + modalityName + " source stem '"
                + sourceStem + "' found in " + directory.string() + "."
            );
        }

        indexed.emplace(sourceStem, path);
    }

    return indexed;
}

} // namespace

std::string DVTODAdapter::datasetId() const
{
    return kDatasetId;
}

std::vector<DatasetPartitionId>
DVTODAdapter::supportedPartitions() const
{
    return {
        DatasetPartitionId("train"),
        DatasetPartitionId("val")
    };
}

std::filesystem::path
DVTODAdapter::datasetDirectory(
    const std::filesystem::path& datasetRoot
)
{
    return datasetRoot / "raw" / kPhysicalDatasetDirectory;
}

std::filesystem::path
DVTODAdapter::partitionDirectory(
    const std::filesystem::path& datasetRoot,
    const std::string& modalityRoot,
    const DatasetPartitionId& partitionId
)
{
    return datasetDirectory(datasetRoot)
        / modalityRoot
        / kImagesDirectory
        / partitionId.value();
}

void DVTODAdapter::validateLayout(
    const std::filesystem::path& datasetRoot
) const
{
    if (!std::filesystem::exists(datasetRoot)) {
        throw DatasetLayoutError(
            "Shared dataset root does not exist: " + datasetRoot.string()
        );
    }

    if (!std::filesystem::is_directory(datasetRoot)) {
        throw DatasetLayoutError(
            "Shared dataset root is not a directory: " + datasetRoot.string()
        );
    }

    const auto dvtodDirectory = datasetDirectory(datasetRoot);

    if (!std::filesystem::is_directory(dvtodDirectory)) {
        throw DatasetLayoutError(
            "DVTOD dataset directory does not exist: "
            + dvtodDirectory.string()
        );
    }

    std::vector<std::filesystem::path> requiredDirectories;

    for (const DatasetPartitionId& partition : supportedPartitions()) {
        requiredDirectories.push_back(
            partitionDirectory(datasetRoot, kVisibleRoot, partition)
        );
        requiredDirectories.push_back(
            partitionDirectory(datasetRoot, kThermalRoot, partition)
        );
    }

    std::vector<std::filesystem::path> missingDirectories;

    for (const auto& directory : requiredDirectories) {
        if (!std::filesystem::is_directory(directory)) {
            missingDirectories.push_back(directory);
        }
    }

    if (!missingDirectories.empty()) {
        std::string message = "DVTOD is missing required directories:";

        for (const auto& directory : missingDirectories) {
            message += "\n- " + directory.string();
        }

        throw DatasetLayoutError(message);
    }
}

std::string DVTODAdapter::toManifestRelativePath(
    const std::filesystem::path& physicalPath,
    const std::filesystem::path& datasetRoot
)
{
    const auto relativePath = physicalPath.lexically_relative(datasetRoot);

    if (relativePath.empty() || relativePath.is_absolute()) {
        throw DatasetPairingError(
            "DVTOD candidate path could not be made relative to the shared dataset root."
        );
    }

    for (const auto& component : relativePath) {
        if (component == "..") {
            throw DatasetPairingError(
                "DVTOD candidate path escapes the shared dataset root."
            );
        }
    }

    return relativePath.generic_string();
}

std::vector<DatasetPair>
DVTODAdapter::discoverPairs(
    const std::filesystem::path& datasetRoot,
    const DatasetPartitionId& partitionId
) const
{
    if (!supportsPartition(partitionId)) {
        throw DatasetPairingError(
            "DVTOD does not support partition '" + partitionId.value() + "'."
        );
    }

    validateLayout(datasetRoot);

    const auto visibleDirectory =
        partitionDirectory(datasetRoot, kVisibleRoot, partitionId);

    const auto thermalDirectory =
        partitionDirectory(datasetRoot, kThermalRoot, partitionId);

    const auto visibleFiles = indexSupportedFiles(visibleDirectory, "visible");
    const auto thermalFiles = indexSupportedFiles(thermalDirectory, "thermal");

    std::set<std::string> uniqueCandidateStems;

    for (const auto& [stem, path] : visibleFiles) {
        static_cast<void>(path);
        uniqueCandidateStems.insert(stem);
    }

    for (const auto& [stem, path] : thermalFiles) {
        static_cast<void>(path);
        uniqueCandidateStems.insert(stem);
    }

    std::vector<std::string> candidateStems(
        uniqueCandidateStems.begin(),
        uniqueCandidateStems.end()
    );

    std::sort(candidateStems.begin(), candidateStems.end(), naturalStemLess);

    std::vector<DatasetPair> discoveredPairs;
    discoveredPairs.reserve(candidateStems.size());

    for (const std::string& sourceStem : candidateStems) {
        const std::string expectedFilename =
            sourceStem + kSupportedExtension;

        const auto visibleIterator = visibleFiles.find(sourceStem);
        const auto thermalIterator = thermalFiles.find(sourceStem);

        const std::filesystem::path visiblePath =
            visibleIterator != visibleFiles.end()
                ? visibleIterator->second
                : visibleDirectory / expectedFilename;

        const std::filesystem::path thermalPath =
            thermalIterator != thermalFiles.end()
                ? thermalIterator->second
                : thermalDirectory / expectedFilename;

        discoveredPairs.emplace_back(
            datasetId() + "_" + partitionId.value() + "_" + sourceStem,
            datasetId(),
            partitionId,
            sourceStem,
            toManifestRelativePath(visiblePath, datasetRoot),
            toManifestRelativePath(thermalPath, datasetRoot)
        );
    }

    return discoveredPairs;
}

} // namespace qart::core::datasets
