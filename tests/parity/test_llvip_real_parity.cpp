//
// Created by hakgu on 8/20/2026.
//

#include "core/datasets/llvip_adapter.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include "core/manifests/csv_validation_manifest_reader.h"

#include "core/visualization/preview_pair_selector.h"
#include "core/visualization/preview_selection.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

using qart::core::datasets::LLVIPAdapter;

using qart::core::domain::datasets::
    DatasetPair;
using qart::core::domain::datasets::
    DatasetPartitionId;

using qart::core::domain::imaging::
    ImageDimensions;

using qart::core::domain::validation::
    PairValidationResult;
using qart::core::domain::validation::
    PairValidationStatus;

using qart::core::manifests::
    CsvValidationManifestReader;

using qart::core::visualization::
    PreviewPairSelector;
using qart::core::visualization::
    PreviewSelectionRequest;


constexpr std::size_t kExpectedTrainPairs =
    12025;

constexpr std::size_t kExpectedTestPairs =
    3463;

constexpr std::size_t kExpectedTotalPairs =
    15488;


int fail(
    const int code,
    const std::string& message
)
{
    std::cerr
        << "[FAIL] "
        << message
        << '\n';

    return code;
}


bool pairsEqual(
    const DatasetPair& left,
    const DatasetPair& right
)
{
    return
        left.pairId()
            == right.pairId()
        && left.datasetId()
            == right.datasetId()
        && left.partitionId()
            == right.partitionId()
        && left.sourceStem()
            == right.sourceStem()
        && left.visibleRelativePath()
            == right.visibleRelativePath()
        && left.thermalRelativePath()
            == right.thermalRelativePath();
}

} // namespace


int main(
    const int argc,
    char* argv[]
)
{
    if (argc != 2) {
        return fail(
            1,
            "Usage: test_llvip_real_parity "
            "<absolute QART dataset root>"
        );
    }

    const std::filesystem::path datasetRoot =
        std::filesystem::absolute(
            std::filesystem::path(
                argv[1]
            )
        );

    const std::filesystem::path manifestPath =
        datasetRoot
        / "manifests"
        / "llvip"
        / "llvip_validation_schema_2_0.csv";

    if (
        !std::filesystem::is_directory(
            datasetRoot
        )
    ) {
        return fail(
            2,
            "Dataset root does not exist or is not "
            "a directory: "
            + datasetRoot.string()
        );
    }

    if (
        !std::filesystem::is_regular_file(
            manifestPath
        )
    ) {
        return fail(
            3,
            "Frozen LLVIP Schema 2.0 manifest "
            "was not found: "
            + manifestPath.string()
        );
    }

    std::cout
        << "[LLVIP parity] Dataset root: "
        << datasetRoot
        << '\n';

    std::cout
        << "[LLVIP parity] Manifest: "
        << manifestPath
        << '\n';


    // ------------------------------------------------------------
    // 1. Real C++ filesystem discovery.
    // ------------------------------------------------------------

    LLVIPAdapter adapter;

    const auto trainPairs =
        adapter.discoverPairs(
            datasetRoot,
            DatasetPartitionId(
                "train"
            )
        );

    const auto testPairs =
        adapter.discoverPairs(
            datasetRoot,
            DatasetPartitionId(
                "test"
            )
        );

    if (
        trainPairs.size()
        != kExpectedTrainPairs
    ) {
        return fail(
            4,
            "Expected 12025 LLVIP train pairs but found "
            + std::to_string(
                trainPairs.size()
            )
        );
    }

    if (
        testPairs.size()
        != kExpectedTestPairs
    ) {
        return fail(
            5,
            "Expected 3463 LLVIP test pairs but found "
            + std::to_string(
                testPairs.size()
            )
        );
    }

    std::vector<DatasetPair>
        discoveredPairs;

    discoveredPairs.reserve(
        kExpectedTotalPairs
    );

    discoveredPairs.insert(
        discoveredPairs.end(),
        trainPairs.begin(),
        trainPairs.end()
    );

    discoveredPairs.insert(
        discoveredPairs.end(),
        testPairs.begin(),
        testPairs.end()
    );

    if (
        discoveredPairs.size()
        != kExpectedTotalPairs
    ) {
        return fail(
            6,
            "Combined LLVIP discovery does not contain "
            "15488 pairs."
        );
    }


    // ------------------------------------------------------------
    // 2. Read Python-produced canonical Schema 2.0 manifest.
    // ------------------------------------------------------------

    CsvValidationManifestReader reader;

    const auto manifest =
        reader.read(
            manifestPath
        );

    if (
        manifest.schemaVersion()
        != "2.0"
    ) {
        return fail(
            7,
            "Expected canonical manifest Schema 2.0."
        );
    }

    if (
        manifest.rows().size()
        != kExpectedTotalPairs
    ) {
        return fail(
            8,
            "Expected 15488 manifest rows but found "
            + std::to_string(
                manifest.rows().size()
            )
        );
    }

    const auto manifestResults =
        manifest.toResults();

    if (
        manifestResults.size()
        != kExpectedTotalPairs
    ) {
        return fail(
            9,
            "Manifest did not reconstruct 15488 "
            "typed validation results."
        );
    }


    // ------------------------------------------------------------
    // 3. Full ordered DatasetPair parity.
    //
    // This compares every discovered C++ pair against the
    // Python-produced manifest identity, not just counts.
    // ------------------------------------------------------------

    for (
        std::size_t index = 0;
        index < kExpectedTotalPairs;
        ++index
    ) {
        const DatasetPair& discovered =
            discoveredPairs.at(
                index
            );

        const DatasetPair& manifested =
            manifestResults.at(
                index
            ).pair();

        if (
            !pairsEqual(
                discovered,
                manifested
            )
        ) {
            return fail(
                10,
                "Ordered pair parity failed at index "
                + std::to_string(index)
                + ": discovered='"
                + discovered.pairId()
                + "', manifest='"
                + manifested.pairId()
                + "'."
            );
        }
    }


    // ------------------------------------------------------------
    // 4. Frozen validation-metadata parity.
    //
    // Python established all 15488 LLVIP pairs as VALID,
    // both decoded, at 1280 x 1024.
    // ------------------------------------------------------------

    const ImageDimensions expectedDimensions(
        1280,
        1024
    );

    for (
        std::size_t index = 0;
        index < manifestResults.size();
        ++index
    ) {
        const PairValidationResult& result =
            manifestResults.at(
                index
            );

        if (
            result.status()
            != PairValidationStatus::Valid
        ) {
            return fail(
                11,
                "Manifest contains non-VALID LLVIP result: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDecoded()
            || !result.thermalDecoded()
        ) {
            return fail(
                12,
                "VALID LLVIP result does not report both "
                "modalities decoded: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDimensions().has_value()
            || !result.thermalDimensions().has_value()
        ) {
            return fail(
                13,
                "VALID LLVIP result lacks dimensions: "
                + result.pair().pairId()
            );
        }

        if (
            result.visibleDimensions().value()
                != expectedDimensions
            || result.thermalDimensions().value()
                != expectedDimensions
        ) {
            return fail(
                14,
                "LLVIP dimension parity failed for "
                + result.pair().pairId()
            );
        }
    }


    // ------------------------------------------------------------
    // 5. Full-manifest Mode B parity.
    //
    // Frozen Python automatic sample:
    //
    // Train:
    // 010001, 070021, 091058, 140063, 250423
    //
    // Test:
    // 190001, 200039, 220150, 240167, 260536
    // ------------------------------------------------------------

    PreviewPairSelector selector;

    const auto selection =
        selector.select(
            manifestResults,
            PreviewSelectionRequest(
                5
            )
        );

    const std::vector<std::string>
        expectedModeBPairIds = {
            "llvip_train_010001",
            "llvip_train_070021",
            "llvip_train_091058",
            "llvip_train_140063",
            "llvip_train_250423",

            "llvip_test_190001",
            "llvip_test_200039",
            "llvip_test_220150",
            "llvip_test_240167",
            "llvip_test_260536"
        };

    const auto& automaticResults =
        selection.automaticResults();

    if (
        automaticResults.size()
        != expectedModeBPairIds.size()
    ) {
        return fail(
            15,
            "Mode B did not select exactly ten LLVIP pairs."
        );
    }

    for (
        std::size_t index = 0;
        index < expectedModeBPairIds.size();
        ++index
    ) {
        const std::string& observedPairId =
            automaticResults.at(
                index
            )
                .pair()
                .pairId();

        if (
            observedPairId
            != expectedModeBPairIds.at(
                index
            )
        ) {
            return fail(
                16,
                "Mode B parity failed at index "
                + std::to_string(index)
                + ": expected='"
                + expectedModeBPairIds.at(index)
                + "', observed='"
                + observedPairId
                + "'."
            );
        }
    }


    // ------------------------------------------------------------
    // PASS
    // ------------------------------------------------------------

    std::cout
        << "[PASS] LLVIP real-data structural parity\n"
        << "  Train pairs: 12025\n"
        << "  Test pairs: 3463\n"
        << "  Total pairs: 15488\n"
        << "  Manifest rows: 15488\n"
        << "  Validation status: 15488 VALID\n"
        << "  Dimensions: 1280x1024\n"
        << "  Ordered pair parity: exact\n"
        << "  Mode B identity parity: exact\n";

    return 0;
}