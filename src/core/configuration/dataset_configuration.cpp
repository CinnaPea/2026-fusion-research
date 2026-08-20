//
// Created by hakgu on 8/17/2026.
//

#include "core/configuration/dataset_configuration.h"

#include "core/domain/datasets/dataset_exceptions.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace qart::core::configuration {

namespace {

bool isAsciiWhitespace(
    const char character
) noexcept
{
    return character == ' '
        || character == '\t'
        || character == '\n'
        || character == '\r'
        || character == '\f'
        || character == '\v';
}

std::string trimAscii(
    const std::string& value
)
{
    std::size_t first = 0;

    while (
        first < value.size()
        && isAsciiWhitespace(value[first])
    ) {
        ++first;
    }

    std::size_t last = value.size();

    while (
        last > first
        && isAsciiWhitespace(value[last - 1])
    ) {
        --last;
    }

    return value.substr(
        first,
        last - first
    );
}

std::string toLowerAscii(
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

bool isWindowsDriveAbsolute(
    const std::string& path
) noexcept
{
    if (path.size() < 3) {
        return false;
    }

    const unsigned char drive =
        static_cast<unsigned char>(
            path[0]
        );

    return std::isalpha(drive) != 0
        && path[1] == ':'
        && (
            path[2] == '/'
            || path[2] == '\\'
        );
}

bool isWindowsUncAbsolute(
    const std::string& path
) noexcept
{
    return path.size() >= 2
        && (
            (
                path[0] == '\\'
                && path[1] == '\\'
            )
            || (
                path[0] == '/'
                && path[1] == '/'
            )
        );
}

bool isPortableAbsolutePath(
    const std::string& path
)
{
    if (path.empty()) {
        return false;
    }

    return path.front() == '/'
        || isWindowsDriveAbsolute(path)
        || isWindowsUncAbsolute(path)
        || std::filesystem::path(path).is_absolute();
}

bool containsParentTraversal(
    const std::string& path
) noexcept
{
    std::size_t componentStart = 0;

    while (componentStart <= path.size()) {
        const std::size_t separator =
            path.find(
                '/',
                componentStart
            );

        const std::size_t componentLength =
            separator == std::string::npos
                ? path.size() - componentStart
                : separator - componentStart;

        if (
            componentLength == 2
            && path.compare(
                componentStart,
                componentLength,
                ".."
            ) == 0
        ) {
            return true;
        }

        if (separator == std::string::npos) {
            break;
        }

        componentStart =
            separator + 1;
    }

    return false;
}

std::string extensionOfPortablePath(
    const std::string& path
)
{
    return std::filesystem::path(
        path
    ).extension().string();
}

} // namespace

DatasetConfiguration::DatasetConfiguration(
    std::string schemaVersion,
    std::string datasetId,
    std::filesystem::path datasetRoot,
    std::string manifestRelativePath,
    std::vector<
        domain::datasets::DatasetPartitionId
    > supportedPartitions,
    domain::validation::PairValidationPolicy validationPolicy
)
    : schemaVersion_(
          std::move(schemaVersion)
      ),
      datasetId_(
          std::move(datasetId)
      ),
      datasetRoot_(
          std::move(datasetRoot)
      ),
      manifestRelativePath_(
          std::move(manifestRelativePath)
      ),
      supportedPartitions_(
          std::move(supportedPartitions)
      ),
      validationPolicy_(
          std::move(validationPolicy)
      )
{
    validate();
}

const std::string&
DatasetConfiguration::schemaVersion() const noexcept
{
    return schemaVersion_;
}

const std::string&
DatasetConfiguration::datasetId() const noexcept
{
    return datasetId_;
}

const std::filesystem::path&
DatasetConfiguration::datasetRoot() const noexcept
{
    return datasetRoot_;
}

const std::string&
DatasetConfiguration::manifestRelativePath() const noexcept
{
    return manifestRelativePath_;
}

const std::vector<
    domain::datasets::DatasetPartitionId
>& DatasetConfiguration::supportedPartitions() const noexcept
{
    return supportedPartitions_;
}

const domain::validation::PairValidationPolicy&
DatasetConfiguration::validationPolicy() const noexcept
{
    return validationPolicy_;
}

bool DatasetConfiguration::supportsPartition(
    const domain::datasets::DatasetPartitionId& partitionId
) const noexcept
{
    return std::find(
        supportedPartitions_.begin(),
        supportedPartitions_.end(),
        partitionId
    ) != supportedPartitions_.end();
}

bool DatasetConfiguration::supportsPartition(
    const std::string& partitionId
) const
{
    return supportsPartition(
        domain::datasets::DatasetPartitionId(
            partitionId
        )
    );
}

std::filesystem::path
DatasetConfiguration::resolveManifestPath() const
{
    std::filesystem::path resolved =
        datasetRoot_;

    std::size_t componentStart = 0;

    while (
        componentStart
        <= manifestRelativePath_.size()
    ) {
        const std::size_t separator =
            manifestRelativePath_.find(
                '/',
                componentStart
            );

        const std::size_t componentLength =
            separator == std::string::npos
                ? manifestRelativePath_.size()
                    - componentStart
                : separator
                    - componentStart;

        resolved /=
            manifestRelativePath_.substr(
                componentStart,
                componentLength
            );

        if (separator == std::string::npos) {
            break;
        }

        componentStart =
            separator + 1;
    }

    return resolved;
}

void DatasetConfiguration::validate() const
{
    using domain::datasets::DatasetConfigurationError;

    if (
        schemaVersion_
        != kDatasetConfigurationSchemaVersion
    ) {
        throw DatasetConfigurationError(
            "Unsupported dataset configuration "
            "schema version: '"
            + schemaVersion_
            + "'. C++ supports canonical Schema 2.0."
        );
    }

    const std::string trimmedDatasetId =
        trimAscii(
            datasetId_
        );

    if (trimmedDatasetId.empty()) {
        throw DatasetConfigurationError(
            "Dataset configuration dataset_id "
            "must not be empty."
        );
    }

    if (
        datasetId_
        != toLowerAscii(
            trimmedDatasetId
        )
    ) {
        throw DatasetConfigurationError(
            "Dataset configuration dataset_id "
            "must be a canonical lowercase identifier "
            "without surrounding whitespace."
        );
    }

    const std::string datasetRootText =
        datasetRoot_.string();

    if (
        !isPortableAbsolutePath(
            datasetRootText
        )
    ) {
        throw DatasetConfigurationError(
            "Dataset root must be an absolute "
            "filesystem path: "
            + datasetRootText
        );
    }

    if (
        isPortableAbsolutePath(
            manifestRelativePath_
        )
    ) {
        throw DatasetConfigurationError(
            "Manifest path must be relative "
            "to the dataset root."
        );
    }

    if (
        containsParentTraversal(
            manifestRelativePath_
        )
    ) {
        throw DatasetConfigurationError(
            "Manifest path must not contain "
            "parent traversal."
        );
    }

    if (
        manifestRelativePath_.find('\\')
        != std::string::npos
    ) {
        throw DatasetConfigurationError(
            "Manifest path must use forward slashes."
        );
    }

    if (
        toLowerAscii(
            extensionOfPortablePath(
                manifestRelativePath_
            )
        )
        != ".csv"
    ) {
        throw DatasetConfigurationError(
            "Manifest path must use the .csv extension."
        );
    }

    if (supportedPartitions_.empty()) {
        throw DatasetConfigurationError(
            "Dataset configuration must contain "
            "at least one partition."
        );
    }

    const std::set<
        domain::datasets::DatasetPartitionId
    > uniquePartitions(
        supportedPartitions_.begin(),
        supportedPartitions_.end()
    );

    if (
        uniquePartitions.size()
        != supportedPartitions_.size()
    ) {
        throw DatasetConfigurationError(
            "Dataset configuration must not contain "
            "duplicate partitions."
        );
    }
}

bool operator==(
    const DatasetConfiguration& left,
    const DatasetConfiguration& right
) noexcept
{
    return left.schemaVersion_
            == right.schemaVersion_
        && left.datasetId_
            == right.datasetId_
        && left.datasetRoot_
            == right.datasetRoot_
        && left.manifestRelativePath_
            == right.manifestRelativePath_
        && left.supportedPartitions_
            == right.supportedPartitions_
        && left.validationPolicy_
            == right.validationPolicy_;
}

bool operator!=(
    const DatasetConfiguration& left,
    const DatasetConfiguration& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::configuration