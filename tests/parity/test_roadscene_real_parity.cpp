//
// Created by hakgu on 8/21/2026.
//

#include "core/datasets/roadscene_adapter.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include "core/manifests/csv_validation_manifest_reader.h"

#include "core/visualization/preview_pair_selector.h"
#include "core/visualization/preview_selection.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using qart::core::datasets::RoadSceneAdapter;

using qart::core::domain::datasets::
    DatasetPair;
using qart::core::domain::datasets::
    DatasetPartitionId;

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


constexpr std::size_t kExpectedPairs =
    221;

constexpr std::size_t kExpectedDistinctDimensions =
    218;


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
            "Usage: test_roadscene_real_parity "
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
        / "roadscene"
        / "roadscene_validation_schema_2_0.csv";

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
            "Frozen RoadScene Schema 2.0 manifest "
            "was not found: "
            + manifestPath.string()
        );
    }


    // ------------------------------------------------------------
    // 1. Actual C++ RoadScene discovery.
    // ------------------------------------------------------------

    RoadSceneAdapter adapter;

    const auto discoveredPairs =
        adapter.discoverPairs(
            datasetRoot,
            DatasetPartitionId(
                "all"
            )
        );

    if (
        discoveredPairs.size()
        != kExpectedPairs
    ) {
        return fail(
            4,
            "Expected 221 RoadScene pairs but found "
            + std::to_string(
                discoveredPairs.size()
            )
        );
    }


    // ------------------------------------------------------------
    // 2. Every discovered pair must retain native partition "all".
    // ------------------------------------------------------------

    for (
        const DatasetPair& pair
        : discoveredPairs
    ) {
        if (
            pair.partitionId()
            != DatasetPartitionId(
                "all"
            )
        ) {
            return fail(
                5,
                "RoadScene pair lost native partition 'all': "
                + pair.pairId()
            );
        }

        if (
            pair.datasetId()
            != "roadscene"
        ) {
            return fail(
                6,
                "Unexpected RoadScene dataset ID: "
                + pair.datasetId()
            );
        }
    }


    // ------------------------------------------------------------
    // 3. Frozen first / last identities.
    // ------------------------------------------------------------

    if (
        discoveredPairs.front().pairId()
        != "roadscene_all_FLIR_00006"
    ) {
        return fail(
            7,
            "Unexpected first RoadScene pair: "
            + discoveredPairs.front().pairId()
        );
    }

    if (
        discoveredPairs.back().pairId()
        != "roadscene_all_FLIR_video_04215"
    ) {
        return fail(
            8,
            "Unexpected last RoadScene pair: "
            + discoveredPairs.back().pairId()
        );
    }


    // ------------------------------------------------------------
    // 4. Read Python-produced canonical manifest.
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
            9,
            "Expected RoadScene manifest Schema 2.0."
        );
    }

    if (
        manifest.rows().size()
        != kExpectedPairs
    ) {
        return fail(
            10,
            "Expected 221 RoadScene manifest rows but found "
            + std::to_string(
                manifest.rows().size()
            )
        );
    }

    const auto manifestResults =
        manifest.toResults();

    if (
        manifestResults.size()
        != kExpectedPairs
    ) {
        return fail(
            11,
            "RoadScene manifest did not reconstruct "
            "221 typed results."
        );
    }


    // ------------------------------------------------------------
    // 5. Exact ordered DatasetPair parity.
    // ------------------------------------------------------------

    for (
        std::size_t index = 0;
        index < kExpectedPairs;
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
                12,
                "RoadScene ordered pair parity failed "
                "at index "
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
    // 6. Validation metadata.
    //
    // RoadScene intentionally has no global expected dimensions.
    // Each canonical pair must instead report equal decoded
    // visible and thermal dimensions.
    // ------------------------------------------------------------

    std::set<
        std::pair<int, int>
    > observedDimensionPairs;

    for (
        const PairValidationResult& result
        : manifestResults
    ) {
        if (
            result.status()
            != PairValidationStatus::Valid
        ) {
            return fail(
                13,
                "RoadScene manifest contains non-VALID pair: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDecoded()
            || !result.thermalDecoded()
        ) {
            return fail(
                14,
                "VALID RoadScene pair does not report "
                "both sources decoded: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDimensions().has_value()
            || !result.thermalDimensions().has_value()
        ) {
            return fail(
                15,
                "VALID RoadScene pair lacks dimensions: "
                + result.pair().pairId()
            );
        }

        const auto& visibleDimensions =
            result
                .visibleDimensions()
                .value();

        const auto& thermalDimensions =
            result
                .thermalDimensions()
                .value();

        if (
            visibleDimensions
            != thermalDimensions
        ) {
            return fail(
                16,
                "RoadScene canonical pair has mismatching "
                "visible/thermal dimensions: "
                + result.pair().pairId()
            );
        }

        observedDimensionPairs.emplace(
            visibleDimensions.width(),
            visibleDimensions.height()
        );
    }


    // ------------------------------------------------------------
    // 7. Preserve the real variable-resolution structure.
    //
    // 218 distinct dimensions among 221 pairs proves the C++
    // path has NOT silently imposed a global resolution.
    // ------------------------------------------------------------

    if (
        observedDimensionPairs.size()
        != kExpectedDistinctDimensions
    ) {
        return fail(
            17,
            "Expected 218 distinct RoadScene dimensions "
            "but observed "
            + std::to_string(
                observedDimensionPairs.size()
            )
            + "."
        );
    }


    // ------------------------------------------------------------
    // 8. Canonical path policy.
    //
    // crop_LR_visible = canonical visible
    // cropinfrared    = canonical thermal
    // ------------------------------------------------------------

    for (
        const DatasetPair& pair
        : discoveredPairs
    ) {
        if (
            pair.visibleRelativePath().find(
                "raw/roadscene/crop_LR_visible/"
            )
            != 0
        ) {
            return fail(
                18,
                "RoadScene visible path is not canonical: "
                + pair.visibleRelativePath()
            );
        }

        if (
            pair.thermalRelativePath().find(
                "raw/roadscene/cropinfrared/"
            )
            != 0
        ) {
            return fail(
                19,
                "RoadScene thermal path is not canonical: "
                + pair.thermalRelativePath()
            );
        }
    }


    // ------------------------------------------------------------
    // 9. Frozen Mode B identity parity.
    //
    // One native partition => five automatic selections total.
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
            "roadscene_all_FLIR_00006",
            "roadscene_all_FLIR_04726",
            "roadscene_all_FLIR_06660",
            "roadscene_all_FLIR_08220",
            "roadscene_all_FLIR_video_04215"
        };

    const auto& automaticResults =
        selection.automaticResults();

    if (
        automaticResults.size()
        != expectedModeBPairIds.size()
    ) {
        return fail(
            20,
            "Mode B did not select exactly five "
            "RoadScene pairs."
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
                21,
                "RoadScene Mode B parity failed at index "
                + std::to_string(index)
                + ": expected='"
                + expectedModeBPairIds.at(index)
                + "', observed='"
                + observedPairId
                + "'."
            );
        }
    }


    std::cout
        << "[PASS] RoadScene real-data structural parity\n"
        << "  Native partition: all\n"
        << "  Total pairs: 221\n"
        << "  Validation: 221 VALID\n"
        << "  Canonical visible: crop_LR_visible\n"
        << "  Canonical thermal: cropinfrared\n"
        << "  Pairwise dimensions: equal\n"
        << "  Distinct dimensions: 218\n"
        << "  Global fixed dimensions: none\n"
        << "  Ordered pair parity: exact\n"
        << "  Mode B identity parity: exact\n"
        << "  Registration claim: NOT implied\n";

    return 0;
}