//
// Created by hakgu on 8/18/2026.
//

#include "core/configuration/json_experiment_view_loader.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/datasets/dataset_usage_role.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::configuration::
    JsonExperimentViewLoader;

using qart::core::domain::datasets::
    DatasetConfigurationError;
using qart::core::domain::datasets::
    DatasetPartitionId;
using qart::core::domain::datasets::
    DatasetUsageRole;

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
            std::filesystem::temp_directory_path()
            / "qart_json_experiment_view_contract"
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

bool loadingFails(
    const JsonExperimentViewLoader& loader,
    const std::filesystem::path& path,
    const std::string& content
)
{
    writeText(
        path,
        content
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

    JsonExperimentViewLoader loader;

    // ------------------------------------------------------------
    // LLVIP
    // ------------------------------------------------------------

    const std::filesystem::path llvipPath =
        root / "llvip_default.json";

    writeText(
        llvipPath,
        R"({
            "schema_version": "1.0",
            "view_id": "llvip_default",
            "assignments": [
                {
                    "dataset_id": "llvip",
                    "partition_id": "train",
                    "usage_role": "training"
                },
                {
                    "dataset_id": "llvip",
                    "partition_id": "test",
                    "usage_role": "testing"
                }
            ]
        })"
    );

    const auto llvip =
        loader.load(
            llvipPath
        );

    if (
        llvip.roleFor(
            "llvip",
            DatasetPartitionId("train")
        )
        != DatasetUsageRole::Training
    ) {
        return 1;
    }

    if (
        llvip.roleFor(
            "llvip",
            DatasetPartitionId("test")
        )
        != DatasetUsageRole::Testing
    ) {
        return 2;
    }

    // ------------------------------------------------------------
    // MSRS
    // ------------------------------------------------------------

    const std::filesystem::path msrsPath =
        root / "msrs_default.json";

    writeText(
        msrsPath,
        R"({
            "schema_version": "1.0",
            "view_id": "msrs_default",
            "assignments": [
                {
                    "dataset_id": "msrs",
                    "partition_id": "train",
                    "usage_role": "training"
                },
                {
                    "dataset_id": "msrs",
                    "partition_id": "test",
                    "usage_role": "testing"
                }
            ]
        })"
    );

    const auto msrs =
        loader.load(
            msrsPath
        );

    if (
        msrs.roleFor(
            "msrs",
            DatasetPartitionId("train")
        )
        != DatasetUsageRole::Training
    ) {
        return 3;
    }

    if (
        msrs.roleFor(
            "msrs",
            DatasetPartitionId("test")
        )
        != DatasetUsageRole::Testing
    ) {
        return 4;
    }

    // ------------------------------------------------------------
    // RoadScene:
    // native partition "all" remains distinct from BENCHMARK role.
    // ------------------------------------------------------------

    const std::filesystem::path roadscenePath =
        root / "roadscene_benchmark.json";

    writeText(
        roadscenePath,
        R"({
            "schema_version": "1.0",
            "view_id": "roadscene_benchmark",
            "assignments": [
                {
                    "dataset_id": "roadscene",
                    "partition_id": "all",
                    "usage_role": "benchmark"
                }
            ]
        })"
    );

    const auto roadscene =
        loader.load(
            roadscenePath
        );

    if (
        roadscene.roleFor(
            "roadscene",
            DatasetPartitionId("all")
        )
        != DatasetUsageRole::Benchmark
    ) {
        return 5;
    }

    // ------------------------------------------------------------
    // Empty assignments are valid domain state.
    // Unknown fields are not rejected merely for being unknown.
    // ------------------------------------------------------------

    const std::filesystem::path emptyPath =
        root / "empty.json";

    writeText(
        emptyPath,
        R"({
            "schema_version": "1.0",
            "view_id": "empty_view",
            "assignments": [],
            "future_metadata": "ignored"
        })"
    );

    const auto emptyView =
        loader.load(
            emptyPath
        );

    try {
        static_cast<void>(
            emptyView.roleFor(
                "llvip",
                DatasetPartitionId("train")
            )
        );

        return 6;
    }
    catch (const std::out_of_range&) {
    }

    // ------------------------------------------------------------
    // Invalid JSON
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "invalid.json",
            R"({"schema_version": "1.0", invalid})"
        )
    ) {
        return 7;
    }

    // ------------------------------------------------------------
    // JSON root must be object
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "array-root.json",
            R"(["llvip_default"])"
        )
    ) {
        return 8;
    }

    // ------------------------------------------------------------
    // Schema version
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "schema2.json",
            R"({
                "schema_version": "2.0",
                "view_id": "llvip_default",
                "assignments": []
            })"
        )
    ) {
        return 9;
    }

    // ------------------------------------------------------------
    // Missing view_id
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "missing-view-id.json",
            R"({
                "schema_version": "1.0",
                "assignments": []
            })"
        )
    ) {
        return 10;
    }

    // ------------------------------------------------------------
    // assignments must be array
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "wrong-assignments-type.json",
            R"({
                "schema_version": "1.0",
                "view_id": "broken_view",
                "assignments": {}
            })"
        )
    ) {
        return 11;
    }

    // ------------------------------------------------------------
    // assignment must be object
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "assignment-not-object.json",
            R"({
                "schema_version": "1.0",
                "view_id": "broken_view",
                "assignments": ["llvip"]
            })"
        )
    ) {
        return 12;
    }

    // ------------------------------------------------------------
    // malformed partition
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "bad-partition.json",
            R"({
                "schema_version": "1.0",
                "view_id": "bad_partition",
                "assignments": [
                    {
                        "dataset_id": "roadscene",
                        "partition_id": "RoadScene All",
                        "usage_role": "benchmark"
                    }
                ]
            })"
        )
    ) {
        return 13;
    }

    // ------------------------------------------------------------
    // unsupported usage role
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "bad-role.json",
            R"({
                "schema_version": "1.0",
                "view_id": "bad_role",
                "assignments": [
                    {
                        "dataset_id": "roadscene",
                        "partition_id": "all",
                        "usage_role": "evaluation"
                    }
                ]
            })"
        )
    ) {
        return 14;
    }

    // ------------------------------------------------------------
    // duplicate dataset + partition
    // ------------------------------------------------------------

    if (
        !loadingFails(
            loader,
            root / "duplicate.json",
            R"({
                "schema_version": "1.0",
                "view_id": "duplicate_view",
                "assignments": [
                    {
                        "dataset_id": "roadscene",
                        "partition_id": "all",
                        "usage_role": "benchmark"
                    },
                    {
                        "dataset_id": "roadscene",
                        "partition_id": "all",
                        "usage_role": "testing"
                    }
                ]
            })"
        )
    ) {
        return 15;
    }

    // ------------------------------------------------------------
    // missing physical file
    // ------------------------------------------------------------

    try {
        static_cast<void>(
            loader.load(
                root / "does-not-exist.json"
            )
        );

        return 16;
    }
    catch (const DatasetConfigurationError&) {
    }

    return 0;
}