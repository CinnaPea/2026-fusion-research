//
// Created by hakgu on 8/18/2026.
//

#include "core/configuration/json_experiment_view_loader.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/datasets/dataset_usage_role.h"
#include "core/domain/datasets/experiment_view_assignment.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace qart::core::configuration {

namespace {

using Json = nlohmann::json;

using domain::datasets::DatasetConfigurationError;
using domain::datasets::DatasetPartitionId;
using domain::datasets::DatasetUsageRole;
using domain::datasets::ExperimentView;
using domain::datasets::ExperimentViewAssignment;
using domain::datasets::datasetUsageRoleFromString;

std::string requiredString(
    const Json& object,
    const std::string& fieldName
)
{
    const auto iterator =
        object.find(fieldName);

    if (iterator == object.end()) {
        throw DatasetConfigurationError(
            "Experiment-view configuration is missing "
            "required field: "
            + fieldName
        );
    }

    if (!iterator->is_string()) {
        throw DatasetConfigurationError(
            "Experiment-view field '"
            + fieldName
            + "' must be a string."
        );
    }

    const std::string value =
        iterator->get<std::string>();

    if (value.empty()) {
        throw DatasetConfigurationError(
            "Experiment-view field '"
            + fieldName
            + "' must not be empty."
        );
    }

    return value;
}

ExperimentViewAssignment parseAssignment(
    const Json& assignment,
    const std::size_t index
)
{
    if (!assignment.is_object()) {
        throw DatasetConfigurationError(
            "Experiment-view assignment at index "
            + std::to_string(index)
            + " must be a JSON object."
        );
    }

    const std::string datasetId =
        requiredString(
            assignment,
            "dataset_id"
        );

    const std::string partitionValue =
        requiredString(
            assignment,
            "partition_id"
        );

    const std::string usageRoleValue =
        requiredString(
            assignment,
            "usage_role"
        );

    try {
        return ExperimentViewAssignment(
            datasetId,
            DatasetPartitionId(
                partitionValue
            ),
            datasetUsageRoleFromString(
                usageRoleValue
            )
        );
    }
    catch (const std::invalid_argument&) {
        throw DatasetConfigurationError(
            "Experiment-view assignment at index "
            + std::to_string(index)
            + " contains invalid values."
        );
    }
}

std::vector<ExperimentViewAssignment>
parseAssignments(
    const Json& configuration
)
{
    const auto iterator =
        configuration.find(
            "assignments"
        );

    if (iterator == configuration.end()) {
        throw DatasetConfigurationError(
            "Experiment-view configuration is missing "
            "required field: assignments"
        );
    }

    if (!iterator->is_array()) {
        throw DatasetConfigurationError(
            "Experiment-view field 'assignments' "
            "must be an array."
        );
    }

    std::vector<ExperimentViewAssignment>
        assignments;

    assignments.reserve(
        iterator->size()
    );

    std::size_t index = 0;

    for (const Json& assignment : *iterator) {
        assignments.push_back(
            parseAssignment(
                assignment,
                index
            )
        );

        ++index;
    }

    return assignments;
}

} // namespace

domain::datasets::ExperimentView
JsonExperimentViewLoader::load(
    const std::filesystem::path& configPath
) const
{
    if (
        !std::filesystem::is_regular_file(
            configPath
        )
    ) {
        throw DatasetConfigurationError(
            "Experiment-view configuration file "
            "does not exist: "
            + configPath.string()
        );
    }

    std::ifstream configFile(
        configPath,
        std::ios::binary
    );

    if (!configFile) {
        throw DatasetConfigurationError(
            "Experiment-view configuration could "
            "not be read: "
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
            "Experiment-view configuration contains "
            "invalid JSON: "
            + configPath.string()
        );
    }
    catch (
        const nlohmann::json::exception&
    ) {
        throw DatasetConfigurationError(
            "Experiment-view configuration could "
            "not be read: "
            + configPath.string()
        );
    }

    if (!configuration.is_object()) {
        throw DatasetConfigurationError(
            "Experiment-view configuration root "
            "must be a JSON object."
        );
    }

    const std::string schemaVersion =
        requiredString(
            configuration,
            "schema_version"
        );

    if (
        schemaVersion
        != kExperimentViewSchemaVersion
    ) {
        throw DatasetConfigurationError(
            "Unsupported experiment-view schema "
            "version: '"
            + schemaVersion
            + "'. C++ supports Schema 1.0."
        );
    }

    const std::string viewId =
        requiredString(
            configuration,
            "view_id"
        );

    std::vector<ExperimentViewAssignment>
        assignments =
            parseAssignments(
                configuration
            );

    try {
        return ExperimentView(
            viewId,
            std::move(assignments)
        );
    }
    catch (const std::invalid_argument&) {
        throw DatasetConfigurationError(
            "Experiment-view configuration "
            "contains invalid values."
        );
    }
}

} // namespace qart::core::configuration