//
// Created by hakgu on 8/17/2026.
//

#include "core/configuration/json_dataset_configuration_loader.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_policy.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace qart::core::configuration {

namespace {

using Json = nlohmann::json;

using domain::datasets::DatasetConfigurationError;
using domain::datasets::DatasetPartitionId;
using domain::imaging::ImageDimensions;
using domain::validation::PairValidationPolicy;

const std::set<std::string> requiredFields = {
    "schema_version",
    "dataset_id",
    "dataset_root",
    "manifest_relative_path",
    "supported_partitions",
    "supported_visible_extensions",
    "supported_thermal_extensions",
    "expected_visible_dimensions",
    "expected_thermal_dimensions"
};

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

bool isWhitespaceOnly(
    const std::string& value
) noexcept
{
    for (const char character : value) {
        if (!isAsciiWhitespace(character)) {
            return false;
        }
    }

    return true;
}

std::string joinStrings(
    const std::set<std::string>& values
)
{
    std::string result;

    for (const std::string& value : values) {
        if (!result.empty()) {
            result += ", ";
        }

        result += value;
    }

    return result;
}

std::string requiredString(
    const Json& configuration,
    const std::string& fieldName
)
{
    const auto iterator =
        configuration.find(fieldName);

    if (iterator == configuration.end()) {
        throw DatasetConfigurationError(
            "Dataset configuration is missing required fields: "
            + fieldName
        );
    }

    if (!iterator->is_string()) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' must be a string."
        );
    }

    const std::string value =
        iterator->get<std::string>();

    if (
        value.empty()
        || isWhitespaceOnly(value)
    ) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' must not be empty."
        );
    }

    return value;
}

std::vector<std::string> requiredStringArray(
    const Json& configuration,
    const std::string& fieldName
)
{
    const auto iterator =
        configuration.find(fieldName);

    if (iterator == configuration.end()) {
        throw DatasetConfigurationError(
            "Dataset configuration is missing required fields: "
            + fieldName
        );
    }

    if (!iterator->is_array()) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' must be an array."
        );
    }

    if (iterator->empty()) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' must not be empty."
        );
    }

    std::vector<std::string> parsedValues;

    parsedValues.reserve(
        iterator->size()
    );

    std::size_t index = 0;

    for (const Json& item : *iterator) {
        if (!item.is_string()) {
            throw DatasetConfigurationError(
                "Configuration field '"
                + fieldName
                + "' contains an invalid string at index "
                + std::to_string(index)
                + "."
            );
        }

        const std::string value =
            item.get<std::string>();

        if (
            value.empty()
            || isWhitespaceOnly(value)
        ) {
            throw DatasetConfigurationError(
                "Configuration field '"
                + fieldName
                + "' contains an invalid string at index "
                + std::to_string(index)
                + "."
            );
        }

        parsedValues.push_back(
            value
        );

        ++index;
    }

    return parsedValues;
}

int jsonIntegerToInt(
    const Json& value,
    const std::string& fieldName
)
{
    if (
        value.is_boolean()
        || (
            !value.is_number_integer()
            && !value.is_number_unsigned()
        )
    ) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' width and height must be integers."
        );
    }

    if (value.is_number_unsigned()) {
        const std::uint64_t parsed =
            value.get<std::uint64_t>();

        if (
            parsed
            > static_cast<std::uint64_t>(
                std::numeric_limits<int>::max()
            )
        ) {
            throw DatasetConfigurationError(
                "Configuration field '"
                + fieldName
                + "' contains an integer outside "
                  "the supported C++ range."
            );
        }

        return static_cast<int>(
            parsed
        );
    }

    const std::int64_t parsed =
        value.get<std::int64_t>();

    if (
        parsed
            < static_cast<std::int64_t>(
                std::numeric_limits<int>::min()
            )
        || parsed
            > static_cast<std::int64_t>(
                std::numeric_limits<int>::max()
            )
    ) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' contains an integer outside "
              "the supported C++ range."
        );
    }

    return static_cast<int>(
        parsed
    );
}

std::optional<ImageDimensions> optionalDimensions(
    const Json& configuration,
    const std::string& fieldName
)
{
    const auto iterator =
        configuration.find(fieldName);

    if (iterator == configuration.end()) {
        throw DatasetConfigurationError(
            "Dataset configuration is missing required fields: "
            + fieldName
        );
    }

    if (iterator->is_null()) {
        return std::nullopt;
    }

    if (!iterator->is_object()) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' must be an object or null."
        );
    }

    const std::set<std::string>
        expectedDimensionFields = {
            "width",
            "height"
        };

    std::set<std::string>
        observedDimensionFields;

    for (
        auto fieldIterator = iterator->begin();
        fieldIterator != iterator->end();
        ++fieldIterator
    ) {
        observedDimensionFields.insert(
            fieldIterator.key()
        );
    }

    if (
        observedDimensionFields
        != expectedDimensionFields
    ) {
        throw DatasetConfigurationError(
            "Configuration field '"
            + fieldName
            + "' must contain exactly "
              "'width' and 'height'."
        );
    }

    const Json& widthValue =
        iterator->at("width");

    const Json& heightValue =
        iterator->at("height");

    const int width =
        jsonIntegerToInt(
            widthValue,
            fieldName
        );

    const int height =
        jsonIntegerToInt(
            heightValue,
            fieldName
        );
    return ImageDimensions(
        width,
        height
    );

}

std::vector<DatasetPartitionId> parsePartitions(
    const Json& configuration
)
{
    const std::vector<std::string>
        partitionValues =
            requiredStringArray(
                configuration,
                "supported_partitions"
            );

    std::vector<DatasetPartitionId>
        partitions;

    partitions.reserve(
        partitionValues.size()
    );

    for (
        const std::string& partitionValue
        : partitionValues
    ) {
        try {
            partitions.emplace_back(
                partitionValue
            );
        }
        catch (const std::invalid_argument&) {
            throw DatasetConfigurationError(
                "Dataset configuration contains an "
                "unsupported partition: '"
                + partitionValue
                + "'."
            );
        }
    }

    return partitions;
}

PairValidationPolicy buildValidationPolicy(
    const std::vector<std::string>& visibleExtensions,
    const std::vector<std::string>& thermalExtensions,
    const std::optional<ImageDimensions>&
        expectedVisibleDimensions,
    const std::optional<ImageDimensions>&
        expectedThermalDimensions
)
{

    return PairValidationPolicy(
        visibleExtensions,
        thermalExtensions,
        expectedVisibleDimensions,
        expectedThermalDimensions
    );
}

void validateExactFieldSet(
    const Json& configuration
)
{
    std::set<std::string> observedFields;

    for (
        auto iterator = configuration.begin();
        iterator != configuration.end();
        ++iterator
    ) {
        observedFields.insert(
            iterator.key()
        );
    }

    std::set<std::string> missingFields;
    std::set<std::string> unknownFields;

    for (
        const std::string& required
        : requiredFields
    ) {
        if (
            observedFields.find(required)
            == observedFields.end()
        ) {
            missingFields.insert(
                required
            );
        }
    }

    for (
        const std::string& observed
        : observedFields
    ) {
        if (
            requiredFields.find(observed)
            == requiredFields.end()
        ) {
            unknownFields.insert(
                observed
            );
        }
    }

    if (!missingFields.empty()) {
        throw DatasetConfigurationError(
            "Dataset configuration is missing required fields: "
            + joinStrings(missingFields)
        );
    }

    if (!unknownFields.empty()) {
        throw DatasetConfigurationError(
            "Dataset configuration contains unknown fields: "
            + joinStrings(unknownFields)
        );
    }
}

} // namespace

DatasetConfiguration
JsonDatasetConfigurationLoader::load(
    const std::filesystem::path& configPath
) {
    if (
        !std::filesystem::is_regular_file(
            configPath
        )
    ) {
        throw DatasetConfigurationError(
            "Dataset configuration file does not exist: "
            + configPath.string()
        );
    }

    std::ifstream configFile(
        configPath,
        std::ios::binary
    );

    if (!configFile) {
        throw DatasetConfigurationError(
            "Dataset configuration could not be read: "
            + configPath.string()
        );
    }

    Json configuration;

    try {
        configFile >> configuration;
    }
    catch (
        const nlohmann::json::parse_error&
    ) {
        throw DatasetConfigurationError(
            "Dataset configuration contains invalid JSON: "
            + configPath.string()
        );
    }
    catch (
        const nlohmann::json::exception&
    ) {
        throw DatasetConfigurationError(
            "Dataset configuration could not be read: "
            + configPath.string()
        );
    }

    if (!configuration.is_object()) {
        throw DatasetConfigurationError(
            "Dataset configuration root must be a JSON object."
        );
    }

    const std::string schemaVersion =
        requiredString(
            configuration,
            "schema_version"
        );

    if (
        schemaVersion
        != kDatasetConfigurationSchemaVersion
    ) {
        throw DatasetConfigurationError(
            "Unsupported dataset configuration "
            "schema version: '"
            + schemaVersion
            + "'. C++ supports canonical Schema 2.0."
        );
    }

    validateExactFieldSet(
        configuration
    );

    const std::string datasetId =
        requiredString(
            configuration,
            "dataset_id"
        );

    const std::string datasetRoot =
        requiredString(
            configuration,
            "dataset_root"
        );

    const std::string manifestRelativePath =
        requiredString(
            configuration,
            "manifest_relative_path"
        );

    const std::vector<std::string>
        visibleExtensions =
            requiredStringArray(
                configuration,
                "supported_visible_extensions"
            );

    const std::vector<std::string>
        thermalExtensions =
            requiredStringArray(
                configuration,
                "supported_thermal_extensions"
            );

    const std::optional<ImageDimensions>
        expectedVisibleDimensions =
            optionalDimensions(
                configuration,
                "expected_visible_dimensions"
            );

    const std::optional<ImageDimensions>
        expectedThermalDimensions =
            optionalDimensions(
                configuration,
                "expected_thermal_dimensions"
            );

    PairValidationPolicy validationPolicy =
        buildValidationPolicy(
            visibleExtensions,
            thermalExtensions,
            expectedVisibleDimensions,
            expectedThermalDimensions
        );

    std::vector<DatasetPartitionId>
        supportedPartitions =
            parsePartitions(
                configuration
            );

    return DatasetConfiguration(
        schemaVersion,
        datasetId,
        std::filesystem::path(
            datasetRoot
        ),
        manifestRelativePath,
        std::move(
            supportedPartitions
        ),
        std::move(
            validationPolicy
        )
    );
}

} // namespace qart::core::configuration