//
// Created by hakgu on 8/21/2026.
//

#include "core/datasets/msrs_adapter.h"

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

using qart::core::datasets::MSRSAdapter;

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
    1083;

constexpr std::size_t kExpectedTestPairs =
    361;

constexpr std::size_t kExpectedTotalPairs =
    1444;

constexpr std::size_t kExpectedTrainDay =
    536;

constexpr std::size_t kExpectedTrainNight =
    547;

constexpr std::size_t kExpectedTestDay =
    179;

constexpr std::size_t kExpectedTestNight =
    182;


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


struct ConditionCounts final
{
    std::size_t day = 0;
    std::size_t night = 0;
};


ConditionCounts countConditions(
    const std::vector<DatasetPair>& pairs
)
{
    ConditionCounts counts;

    for (const DatasetPair& pair : pairs) {
        const std::string& stem =
            pair.sourceStem();

        if (stem.empty()) {
            throw std::runtime_error(
                "MSRS pair contains an empty source stem."
            );
        }

        switch (stem.back()) {
            case 'D':
                ++counts.day;
                break;

            case 'N':
                ++counts.night;
                break;

            default:
                throw std::runtime_error(
                    "MSRS source stem has neither D nor N suffix: "
                    + stem
                );
        }
    }

    return counts;
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
            "Usage: test_msrs_real_parity "
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
        / "msrs"
        / "msrs_validation_schema_2_0.csv";

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
            "Frozen MSRS Schema 2.0 manifest not found: "
            + manifestPath.string()
        );
    }


    // ------------------------------------------------------------
    // 1. Actual MSRS filesystem discovery.
    // ------------------------------------------------------------

    MSRSAdapter adapter;

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
            "Expected 1083 train pairs but found "
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
            "Expected 361 test pairs but found "
            + std::to_string(
                testPairs.size()
            )
        );
    }


    // ------------------------------------------------------------
    // 2. Verify frozen D/N distribution.
    // ------------------------------------------------------------

    ConditionCounts trainConditions;

    ConditionCounts testConditions;

    try {
        trainConditions =
            countConditions(
                trainPairs
            );

        testConditions =
            countConditions(
                testPairs
            );
    }
    catch (const std::exception& error) {
        return fail(
            6,
            error.what()
        );
    }

    if (
        trainConditions.day
            != kExpectedTrainDay
        || trainConditions.night
            != kExpectedTrainNight
    ) {
        return fail(
            7,
            "MSRS train D/N distribution differs "
            "from frozen Python evidence."
        );
    }

    if (
        testConditions.day
            != kExpectedTestDay
        || testConditions.night
            != kExpectedTestNight
    ) {
        return fail(
            8,
            "MSRS test D/N distribution differs "
            "from frozen Python evidence."
        );
    }


    // ------------------------------------------------------------
    // 3. Canonical combined ordering:
    //    train rows followed by test rows.
    // ------------------------------------------------------------

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
            9,
            "Combined MSRS discovery did not contain "
            "1444 pairs."
        );
    }


    // ------------------------------------------------------------
    // 4. Read Python-produced Schema 2.0 manifest.
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
            10,
            "Expected canonical manifest Schema 2.0."
        );
    }

    if (
        manifest.rows().size()
        != kExpectedTotalPairs
    ) {
        return fail(
            11,
            "Expected 1444 MSRS manifest rows but found "
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
            12,
            "Manifest did not reconstruct "
            "1444 typed results."
        );
    }


    // ------------------------------------------------------------
    // 5. Exact ordered pair parity.
    // ------------------------------------------------------------

    for (
        std::size_t index = 0;
        index < discoveredPairs.size();
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
                13,
                "Ordered MSRS pair parity failed at index "
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
    // 6. Frozen validation metadata.
    // ------------------------------------------------------------

    const ImageDimensions expectedDimensions(
        640,
        480
    );

    for (
        const PairValidationResult& result
        : manifestResults
    ) {
        if (
            result.status()
            != PairValidationStatus::Valid
        ) {
            return fail(
                14,
                "Manifest contains non-VALID MSRS pair: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDecoded()
            || !result.thermalDecoded()
        ) {
            return fail(
                15,
                "VALID MSRS pair does not report "
                "both modalities decoded: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDimensions().has_value()
            || !result.thermalDimensions().has_value()
        ) {
            return fail(
                16,
                "VALID MSRS pair lacks dimensions: "
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
                17,
                "MSRS dimension parity failed for "
                + result.pair().pairId()
            );
        }
    }


    // ------------------------------------------------------------
    // 7. Frozen Mode B identity parity.
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
            "msrs_train_00001D",
            "msrs_train_00421D",
            "msrs_train_00852N",
            "msrs_train_01220N",
            "msrs_train_01602D",

            "msrs_test_00004N",
            "msrs_test_00427D",
            "msrs_test_00854N",
            "msrs_test_01222N",
            "msrs_test_01603D"
        };

    const auto& automaticResults =
        selection.automaticResults();

    if (
        automaticResults.size()
        != expectedModeBPairIds.size()
    ) {
        return fail(
            18,
            "Mode B did not select exactly "
            "ten MSRS pairs."
        );
    }

    for (
        std::size_t index = 0;
        index < expectedModeBPairIds.size();
        ++index
    ) {
        const std::string& observed =
            automaticResults.at(
                index
            )
                .pair()
                .pairId();

        if (
            observed
            != expectedModeBPairIds.at(
                index
            )
        ) {
            return fail(
                19,
                "MSRS Mode B parity failed at index "
                + std::to_string(index)
                + ": expected='"
                + expectedModeBPairIds.at(index)
                + "', observed='"
                + observed
                + "'."
            );
        }
    }


    std::cout
        << "[PASS] MSRS real-data structural parity\n"
        << "  Train pairs: 1083\n"
        << "    Day: 536\n"
        << "    Night: 547\n"
        << "  Test pairs: 361\n"
        << "    Day: 179\n"
        << "    Night: 182\n"
        << "  Total pairs: 1444\n"
        << "  Validation: 1444 VALID\n"
        << "  Dimensions: 640x480\n"
        << "  Ordered pair parity: exact\n"
        << "  Mode B identity parity: exact\n";

    return 0;
}