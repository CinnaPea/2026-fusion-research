//
// Created by hakgu on 8/18/2026.
//

#include "core/configuration/json_dataset_configuration_loader.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::configuration::
    JsonDatasetConfigurationLoader;
using qart::core::domain::datasets::
    DatasetConfigurationError;
using qart::core::domain::datasets::
    DatasetPartitionId;
using qart::core::domain::imaging::
    ImageDimensions;

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
            std::filesystem::temp_directory_path()
            / "qart_json_configuration_contract"
        )
    {
        std::error_code error;

        std::filesystem::remove_all(
            path_,
            error
        );

        std::filesystem::create_directories(
            path_
        );
    }

    ~TemporaryDirectory()
    {
        std::error_code error;

        std::filesystem::remove_all(
            path_,
            error
        );
    }

    [[nodiscard]]
    const std::filesystem::path&
    path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void writeText(
    const std::filesystem::path& path,
    const std::string& text
)
{
    std::ofstream stream(
        path,
        std::ios::binary
    );

    stream << text;
}

std::string llvipJson()
{
    return R"({
        "schema_version": "2.0",
        "dataset_id": "llvip",
        "dataset_root": "F:/QART_Datasets",
        "manifest_relative_path":
            "manifests/llvip/llvip_validation_v2.csv",
        "supported_partitions": [
            "train",
            "test"
        ],
        "supported_visible_extensions": [
            ".jpg"
        ],
        "supported_thermal_extensions": [
            ".jpg"
        ],
        "expected_visible_dimensions": {
            "width": 1280,
            "height": 1024
        },
        "expected_thermal_dimensions": {
            "width": 1280,
            "height": 1024
        }
    })";
}

bool loadingFails(
    JsonDatasetConfigurationLoader& loader,
    const std::filesystem::path& path,
    const std::string& json
)
{
    writeText(
        path,
        json
    );

    try {
        static_cast<void>(
            loader.load(path)
        );
    }
    catch (const DatasetConfigurationError&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    TemporaryDirectory temporaryDirectory;

    const std::filesystem::path root =
        temporaryDirectory.path();

    JsonDatasetConfigurationLoader loader;

    const std::filesystem::path llvipPath =
        root / "llvip.json";

    writeText(
        llvipPath,
        llvipJson()
    );

    const auto llvip =
        loader.load(llvipPath);

    if (llvip.schemaVersion() != "2.0") {
        return 1;
    }

    if (llvip.datasetId() != "llvip") {
        return 2;
    }

    if (
        llvip.supportedPartitions()
        != std::vector<DatasetPartitionId>{
            DatasetPartitionId("train"),
            DatasetPartitionId("test")
        }
    ) {
        return 3;
    }

    if (
        !llvip.validationPolicy()
            .expectedVisibleDimensions()
            .has_value()
    ) {
        return 4;
    }

    if (
        llvip.validationPolicy()
            .expectedVisibleDimensions()
            .value()
        != ImageDimensions(1280, 1024)
    ) {
        return 5;
    }

    const std::filesystem::path roadscenePath =
        root / "roadscene.json";

    writeText(
        roadscenePath,
        R"({
            "schema_version": "2.0",
            "dataset_id": "roadscene",
            "dataset_root": "F:/QART_Datasets",
            "manifest_relative_path":
                "manifests/roadscene/roadscene_validation_schema_2_0.csv",
            "supported_partitions": ["all"],
            "supported_visible_extensions": [".jpg"],
            "supported_thermal_extensions": [".jpg"],
            "expected_visible_dimensions": null,
            "expected_thermal_dimensions": null
        })"
    );

    const auto roadscene =
        loader.load(roadscenePath);

    if (
        roadscene.supportedPartitions()
        != std::vector<DatasetPartitionId>{
            DatasetPartitionId("all")
        }
    ) {
        return 6;
    }

    if (
        roadscene.validationPolicy()
            .expectedVisibleDimensions()
            .has_value()
    ) {
        return 7;
    }

    if (
        !loadingFails(
            loader,
            root / "invalid.json",
            R"({"dataset_id": "llvip", invalid})"
        )
    ) {
        return 8;
    }

    if (
        !loadingFails(
            loader,
            root / "array-root.json",
            R"(["llvip", "F:/QART_Datasets"])"
        )
    ) {
        return 9;
    }

    if (
        !loadingFails(
            loader,
            root / "schema1.json",
            R"({
                "schema_version": "1.0",
                "dataset_id": "llvip"
            })"
        )
    ) {
        return 10;
    }

    if (
        !loadingFails(
            loader,
            root / "legacy-field.json",
            R"({
                "schema_version": "2.0",
                "dataset_id": "llvip",
                "dataset_root": "F:/QART_Datasets",
                "manifest_relative_path":
                    "manifests/llvip/test.csv",
                "supported_partitions": ["train"],
                "supported_splits": ["train"],
                "supported_visible_extensions": [".jpg"],
                "supported_thermal_extensions": [".jpg"],
                "expected_visible_dimensions": null,
                "expected_thermal_dimensions": null
            })"
        )
    ) {
        return 11;
    }

    if (
        !loadingFails(
            loader,
            root / "missing-field.json",
            R"({
                "schema_version": "2.0",
                "dataset_id": "llvip",
                "manifest_relative_path":
                    "manifests/llvip/test.csv",
                "supported_partitions": ["train"],
                "supported_visible_extensions": [".jpg"],
                "supported_thermal_extensions": [".jpg"],
                "expected_visible_dimensions": null,
                "expected_thermal_dimensions": null
            })"
        )
    ) {
        return 12;
    }

    if (
        !loadingFails(
            loader,
            root / "boolean-dimension.json",
            R"({
                "schema_version": "2.0",
                "dataset_id": "llvip",
                "dataset_root": "F:/QART_Datasets",
                "manifest_relative_path":
                    "manifests/llvip/test.csv",
                "supported_partitions": ["train"],
                "supported_visible_extensions": [".jpg"],
                "supported_thermal_extensions": [".jpg"],
                "expected_visible_dimensions": {
                    "width": true,
                    "height": 1024
                },
                "expected_thermal_dimensions": null
            })"
        )
    ) {
        return 13;
    }

    if (
        !loadingFails(
            loader,
            root / "malformed-dimension.json",
            R"({
                "schema_version": "2.0",
                "dataset_id": "llvip",
                "dataset_root": "F:/QART_Datasets",
                "manifest_relative_path":
                    "manifests/llvip/test.csv",
                "supported_partitions": ["train"],
                "supported_visible_extensions": [".jpg"],
                "supported_thermal_extensions": [".jpg"],
                "expected_visible_dimensions": {
                    "width": 1280
                },
                "expected_thermal_dimensions": null
            })"
        )
    ) {
        return 14;
    }

    if (
        !loadingFails(
            loader,
            root / "malformed-partition.json",
            R"({
                "schema_version": "2.0",
                "dataset_id": "llvip",
                "dataset_root": "F:/QART_Datasets",
                "manifest_relative_path":
                    "manifests/llvip/test.csv",
                "supported_partitions": [
                    "train",
                    "RoadScene All"
                ],
                "supported_visible_extensions": [".jpg"],
                "supported_thermal_extensions": [".jpg"],
                "expected_visible_dimensions": null,
                "expected_thermal_dimensions": null
            })"
        )
    ) {
        return 15;
    }

    if (
        !loadingFails(
            loader,
            root / "invalid-extension.json",
            R"({
                "schema_version": "2.0",
                "dataset_id": "llvip",
                "dataset_root": "F:/QART_Datasets",
                "manifest_relative_path":
                    "manifests/llvip/test.csv",
                "supported_partitions": ["train"],
                "supported_visible_extensions": ["jpg"],
                "supported_thermal_extensions": [".jpg"],
                "expected_visible_dimensions": null,
                "expected_thermal_dimensions": null
            })"
        )
    ) {
        return 16;
    }

    const std::filesystem::path missingPath =
        root / "does-not-exist.json";

    try {
        static_cast<void>(
            loader.load(missingPath)
        );

        return 17;
    }
    catch (const DatasetConfigurationError&) {
    }

    return 0;
}