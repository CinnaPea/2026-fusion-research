//
// Created by hakgu on 8/17/2026.
//

#include "core/configuration/dataset_configuration.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_policy.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using qart::core::configuration::DatasetConfiguration;
using qart::core::configuration::
    kDatasetConfigurationSchemaVersion;

using qart::core::domain::datasets::
    DatasetConfigurationError;
using qart::core::domain::datasets::
    DatasetPartitionId;
using qart::core::domain::imaging::
    ImageDimensions;
using qart::core::domain::validation::
    PairValidationPolicy;

PairValidationPolicy llvipPolicy()
{
    return PairValidationPolicy(
        {".jpg"},
        {".jpg"},
        ImageDimensions(1280, 1024),
        ImageDimensions(1280, 1024)
    );
}

DatasetConfiguration makeLlVipConfiguration()
{
    return DatasetConfiguration(
        "2.0",
        "llvip",
        std::filesystem::path(
            "F:/QART_Datasets"
        ),
        "manifests/llvip/"
        "llvip_validation_v2.csv",
        {
            DatasetPartitionId("train"),
            DatasetPartitionId("test")
        },
        llvipPolicy()
    );
}

bool rejectsConfiguration(
    const std::string& schemaVersion,
    const std::string& datasetId,
    const std::filesystem::path& datasetRoot,
    const std::string& manifestRelativePath,
    const std::vector<DatasetPartitionId>& partitions
)
{
    try {
        const DatasetConfiguration configuration(
            schemaVersion,
            datasetId,
            datasetRoot,
            manifestRelativePath,
            partitions,
            llvipPolicy()
        );

        static_cast<void>(
            configuration
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
    const DatasetConfiguration llvip =
        makeLlVipConfiguration();

    if (
        llvip.schemaVersion()
        != kDatasetConfigurationSchemaVersion
    ) {
        return 1;
    }

    if (llvip.datasetId() != "llvip") {
        return 2;
    }

    if (
        llvip.datasetRoot()
        != std::filesystem::path(
            "F:/QART_Datasets"
        )
    ) {
        return 3;
    }

    if (
        llvip.supportedPartitions().size()
        != 2
    ) {
        return 4;
    }

    if (
        !llvip.supportsPartition(
            DatasetPartitionId("train")
        )
    ) {
        return 5;
    }

    if (
        !llvip.supportsPartition("test")
    ) {
        return 6;
    }

    if (
        llvip.supportsPartition(
            DatasetPartitionId("all")
        )
    ) {
        return 7;
    }

    const std::filesystem::path expectedManifest =
        std::filesystem::path(
            "F:/QART_Datasets"
        )
        / "manifests"
        / "llvip"
        / "llvip_validation_v2.csv";

    if (
        llvip.resolveManifestPath()
        != expectedManifest
    ) {
        return 8;
    }

    if (
        !llvip.validationPolicy()
             .expectedVisibleDimensions()
             .has_value()
    ) {
        return 9;
    }

    if (
        llvip.validationPolicy()
            .expectedVisibleDimensions()
            .value()
        != ImageDimensions(1280, 1024)
    ) {
        return 10;
    }

    const DatasetConfiguration roadscene(
        "2.0",
        "roadscene",
        std::filesystem::path(
            "F:/QART_Datasets"
        ),
        "manifests/roadscene/"
        "roadscene_validation_schema_2_0.csv",
        {
            DatasetPartitionId("all")
        },
        PairValidationPolicy(
            {".jpg"},
            {".jpg"}
        )
    );

    if (
        !roadscene.supportsPartition("all")
    ) {
        return 11;
    }

    if (
        roadscene.validationPolicy()
            .expectedVisibleDimensions()
            .has_value()
    ) {
        return 12;
    }

    if (
        roadscene.validationPolicy()
            .expectedThermalDimensions()
            .has_value()
    ) {
        return 13;
    }

    const DatasetConfiguration equivalent =
        makeLlVipConfiguration();

    if (llvip != equivalent) {
        return 14;
    }

    if (
        !rejectsConfiguration(
            "1.0",
            "llvip",
            "F:/QART_Datasets",
            "manifests/llvip/test.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 15;
    }

    if (
        !rejectsConfiguration(
            "3.0",
            "llvip",
            "F:/QART_Datasets",
            "manifests/llvip/test.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 16;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "",
            "F:/QART_Datasets",
            "manifests/llvip/test.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 17;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "LLVIP",
            "F:/QART_Datasets",
            "manifests/llvip/test.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 18;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            " llvip",
            "F:/QART_Datasets",
            "manifests/llvip/test.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 19;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "datasets/QART_Datasets",
            "manifests/llvip/test.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 20;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "F:/QART_Datasets",
            "F:/QART_Datasets/manifests/llvip.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 21;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "F:/QART_Datasets",
            "../outside/llvip_validation.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 22;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "F:/QART_Datasets",
            "manifests\\llvip\\validation.csv",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 23;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "F:/QART_Datasets",
            "manifests/llvip/validation.json",
            {
                DatasetPartitionId("train")
            }
        )
    ) {
        return 24;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "F:/QART_Datasets",
            "manifests/llvip/validation.csv",
            {}
        )
    ) {
        return 25;
    }

    if (
        !rejectsConfiguration(
            "2.0",
            "llvip",
            "F:/QART_Datasets",
            "manifests/llvip/validation.csv",
            {
                DatasetPartitionId("train"),
                DatasetPartitionId("train")
            }
        )
    ) {
        return 26;
    }

    return 0;
}