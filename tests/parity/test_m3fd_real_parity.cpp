//
// Created by hakgu on 8/21/2026.
//

#include "core/datasets/m3fd_adapter.h"

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

using qart::core::datasets::M3FDAdapter;

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
    300;

constexpr std::size_t kExpectedDistinctDimensions =
    8;


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
            "Usage: test_m3fd_real_parity "
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
        / "m3fd"
        / "m3fd_validation_schema_2_0.csv";

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
            "Frozen M3FD Schema 2.0 manifest "
            "was not found: "
            + manifestPath.string()
        );
    }


    // ------------------------------------------------------------
    // 1. Actual C++ M3FD discovery.
    // ------------------------------------------------------------

    M3FDAdapter adapter;

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
            "Expected 300 M3FD pairs but found "
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
                "M3FD pair lost native partition 'all': "
                + pair.pairId()
            );
        }

        if (
            pair.datasetId()
            != "m3fd"
        ) {
            return fail(
                6,
                "Unexpected M3FD dataset ID: "
                + pair.datasetId()
            );
        }
    }


    // ------------------------------------------------------------
    // 3. Frozen first / last identities.
    // ------------------------------------------------------------

    if (
        discoveredPairs.front().pairId()
        != "m3fd_all_00000"
    ) {
        return fail(
            7,
            "Unexpected first M3FD pair: "
            + discoveredPairs.front().pairId()
        );
    }

    if (
        discoveredPairs.back().pairId()
        != "m3fd_all_04160"
    ) {
        return fail(
            8,
            "Unexpected last M3FD pair: "
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
            "Expected M3FD manifest Schema 2.0."
        );
    }

    if (
        manifest.rows().size()
        != kExpectedPairs
    ) {
        return fail(
            10,
            "Expected 300 M3FD manifest rows but found "
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
            "M3FD manifest did not reconstruct "
            "300 typed results."
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
                "M3FD ordered pair parity failed "
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
    // M3FD intentionally has no global expected dimensions.
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
                "M3FD manifest contains non-VALID pair: "
                + result.pair().pairId()
            );
        }

        if (
            !result.visibleDecoded()
            || !result.thermalDecoded()
        ) {
            return fail(
                14,
                "VALID M3FD pair does not report "
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
                "VALID M3FD pair lacks dimensions: "
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
                "M3FD canonical pair has mismatching "
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
    // 8 distinct dimensions among 300 pairs proves the C++
    // path has NOT silently imposed a global resolution.
    // ------------------------------------------------------------

    if (
        observedDimensionPairs.size()
        != kExpectedDistinctDimensions
    ) {
        return fail(
            17,
            "Expected 8 distinct M3FD dimensions "
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
                "raw/m3fd/Vis/"
            )
            != 0
        ) {
            return fail(
                18,
                "M3FD visible path is not canonical: "
                + pair.visibleRelativePath()
            );
        }

        if (
            pair.thermalRelativePath().find(
                "raw/m3fd/Ir/"
            )
            != 0
        ) {
            return fail(
                19,
                "M3FD thermal path is not canonical: "
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
            "m3fd_all_00000",
            "m3fd_all_00712",
            "m3fd_all_01505",
            "m3fd_all_02394",
            "m3fd_all_04160"
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
            "M3FD pairs."
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
                "M3FD Mode B parity failed at index "
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
        << "[PASS] M3FD real-data structural parity\n"
        << "  Native partition: all\n"
        << "  Total pairs: 300\n"
        << "  Validation: 300 VALID\n"
        << "  Canonical visible: Vis\n"
        << "  Canonical thermal: Ir\n"
        << "  Pairwise dimensions: equal\n"
        << "  Distinct dimensions: 8\n"
        << "  Global fixed dimensions: none\n"
        << "  Ordered pair parity: exact\n"
        << "  Mode B identity parity: exact\n"
        << "  Registration claim: NOT implied\n";

    return 0;
}