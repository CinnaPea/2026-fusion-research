//
// Created by hakgu on 8/21/2026.
//

#include "core/datasets/tno_adapter.h"

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

using qart::core::datasets::TNOAdapter;

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
    37;

constexpr std::size_t kExpectedDistinctDimensions =
    16;


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
            "Usage: test_tno_real_parity "
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
        / "tno"
        / "tno_validation_schema_2_0.csv";

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
            "Frozen TNO Schema 2.0 manifest "
            "was not found: "
            + manifestPath.string()
        );
    }


    // ------------------------------------------------------------
    // 1. Actual C++ TNO discovery.
    // ------------------------------------------------------------

    TNOAdapter adapter;

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
            "Expected 37 TNO pairs but found "
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
                "TNO pair lost native partition 'all': "
                + pair.pairId()
            );
        }

        if (
            pair.datasetId()
            != "tno"
        ) {
            return fail(
                6,
                "Unexpected TNO dataset ID: "
                + pair.datasetId()
            );
        }
    }


    // ------------------------------------------------------------
    // 3. Frozen first / last identities.
    // ------------------------------------------------------------

    if (
        discoveredPairs.front().pairId()
        != "tno_all_001"
    ) {
        return fail(
            7,
            "Unexpected first TNO pair: "
            + discoveredPairs.front().pairId()
        );
    }

    if (
        discoveredPairs.back().pairId()
        != "tno_all_037"
    ) {
        return fail(
            8,
            "Unexpected last TNO pair: "
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
            "Expected TNO manifest Schema 2.0."
        );
    }

    if (
        manifest.rows().size()
        != kExpectedPairs
    ) {
        return fail(
            10,
            "Expected 37 TNO manifest rows but found "
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
            "TNO manifest did not reconstruct "
            "37 typed results."
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
                "TNO ordered pair parity failed "
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
    // TNO intentionally has no global expected dimensions.
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
                "TNO manifest contains non-VALID pair: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDecoded()
            || !result.thermalDecoded()
        ) {
            return fail(
                14,
                "VALID TNO pair does not report "
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
                "VALID TNO pair lacks dimensions: "
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
                "TNO canonical pair has mismatching "
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
    // 16 distinct dimensions among 37 pairs proves the C++
    // path has NOT silently imposed a global resolution.
    // ------------------------------------------------------------

    if (
        observedDimensionPairs.size()
        != kExpectedDistinctDimensions
    ) {
        return fail(
            17,
            "Expected 16 distinct TNO dimensions "
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
                "raw/tno/vi/"
            )
            != 0
        ) {
            return fail(
                18,
                "TNO visible path is not canonical: "
                + pair.visibleRelativePath()
            );
        }

        if (
            pair.thermalRelativePath().find(
                "raw/tno/ir/"
            )
            != 0
        ) {
            return fail(
                19,
                "TNO thermal path is not canonical: "
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
            "tno_all_001",
            "tno_all_010",
            "tno_all_019",
            "tno_all_028",
            "tno_all_037"
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
            "TNO pairs."
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
                "TNO Mode B parity failed at index "
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
        << "[PASS] TNO real-data structural parity\n"
        << "  Native partition: all\n"
        << "  Total pairs: 37\n"
        << "  Validation: 37 VALID\n"
        << "  Canonical visible: vi\n"
        << "  Canonical thermal: ir\n"
        << "  Pairwise dimensions: equal\n"
        << "  Distinct dimensions: 16\n"
        << "  Global fixed dimensions: none\n"
        << "  Ordered pair parity: exact\n"
        << "  Mode B identity parity: exact\n"
        << "  Registration claim: NOT implied\n";

    return 0;
}