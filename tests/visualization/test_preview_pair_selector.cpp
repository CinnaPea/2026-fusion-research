//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/preview_pair_selector.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <optional>
#include <string>
#include <vector>

namespace {

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

using qart::core::visualization::
    PreviewPairSelector;
using qart::core::visualization::
    PreviewSelectionError;
using qart::core::visualization::
    PreviewSelectionRequest;


PairValidationResult validResult(
    const std::string& datasetId,
    const std::string& partitionId,
    const std::string& sourceStem
)
{
    return PairValidationResult(
        DatasetPair(
            datasetId
                + "_"
                + partitionId
                + "_"
                + sourceStem,
            datasetId,
            DatasetPartitionId(
                partitionId
            ),
            sourceStem,
            "raw/"
                + datasetId
                + "/visible/"
                + sourceStem
                + ".jpg",
            "raw/"
                + datasetId
                + "/thermal/"
                + sourceStem
                + ".jpg"
        ),
        PairValidationStatus::Valid,
        true,
        true,
        ImageDimensions(
            1280,
            1024
        ),
        ImageDimensions(
            1280,
            1024
        )
    );
}


PairValidationResult invalidResult(
    const std::string& sourceStem
)
{
    return PairValidationResult(
        DatasetPair(
            "llvip_train_"
                + sourceStem,
            "llvip",
            DatasetPartitionId(
                "train"
            ),
            sourceStem,
            "raw/llvip/visible/train/"
                + sourceStem
                + ".jpg",
            "raw/llvip/infrared/train/"
                + sourceStem
                + ".jpg"
        ),
        PairValidationStatus::MissingVisible,
        false,
        true,
        std::nullopt,
        ImageDimensions(
            1280,
            1024
        ),
        std::string(
            "Visible source file was not found."
        )
    );
}


bool containsText(
    const std::string& value,
    const std::string& expected
)
{
    return
        value.find(expected)
        != std::string::npos;
}

} // namespace


int main()
{
    PreviewPairSelector selector;

    // ------------------------------------------------------------
    // Exact evenly-spaced Mode B behavior.
    // ------------------------------------------------------------

    std::vector<PairValidationResult>
        distributedResults;

    for (int index = 1; index <= 9; ++index) {
        std::string stem =
            "00000"
            + std::to_string(index);

        distributedResults.push_back(
            validResult(
                "llvip",
                "train",
                stem
            )
        );
    }

    for (int index = 101; index <= 106; ++index) {
        distributedResults.push_back(
            validResult(
                "llvip",
                "test",
                std::to_string(index)
            )
        );
    }

    const auto distributed =
        selector.select(
            distributedResults,
            PreviewSelectionRequest(
                5
            )
        );

    if (
        distributed
            .automaticResults()
            .size()
        != 10
    ) {
        return 1;
    }

    const std::vector<std::string>
        expectedTrainIds = {
            "llvip_train_000001",
            "llvip_train_000003",
            "llvip_train_000005",
            "llvip_train_000007",
            "llvip_train_000009"
        };

    for (
        std::size_t index = 0;
        index < expectedTrainIds.size();
        ++index
    ) {
        if (
            distributed
                .automaticResults()
                .at(index)
                .pair()
                .pairId()
            != expectedTrainIds.at(index)
        ) {
            return 2;
        }
    }

    // ------------------------------------------------------------
    // Automatic mode ignores invalid rows.
    // ------------------------------------------------------------

    const std::vector<PairValidationResult>
        mixedResults = {
            validResult(
                "llvip",
                "train",
                "000001"
            ),
            invalidResult(
                "000002"
            ),
            validResult(
                "llvip",
                "train",
                "000003"
            ),
            validResult(
                "llvip",
                "train",
                "000004"
            )
        };

    const auto mixedSelection =
        selector.select(
            mixedResults,
            PreviewSelectionRequest(
                5
            )
        );

    if (
        mixedSelection
            .automaticResults()
            .size()
        != 3
    ) {
        return 3;
    }

    if (
        mixedSelection
            .automaticResults()
            .at(1)
            .pair()
            .sourceStem()
        != "000003"
    ) {
        return 4;
    }

    // ------------------------------------------------------------
    // Fewer than K => all valid rows.
    // ------------------------------------------------------------

    const std::vector<PairValidationResult>
        lowCountResults = {
            validResult(
                "llvip",
                "test",
                "000001"
            ),
            validResult(
                "llvip",
                "test",
                "000002"
            ),
            validResult(
                "llvip",
                "test",
                "000003"
            )
        };

    const auto lowCount =
        selector.select(
            lowCountResults,
            PreviewSelectionRequest(
                5
            )
        );

    if (
        lowCount
            .automaticResults()
            .size()
        != 3
    ) {
        return 5;
    }

    // ------------------------------------------------------------
    // RoadScene native "all" works identically.
    // ------------------------------------------------------------

    std::vector<PairValidationResult>
        roadsceneResults;

    for (int index = 1; index <= 5; ++index) {
        roadsceneResults.push_back(
            validResult(
                "roadscene",
                "all",
                "FLIR_0000"
                    + std::to_string(
                        index
                    )
            )
        );
    }

    const auto roadsceneSelection =
        selector.select(
            roadsceneResults,
            PreviewSelectionRequest(
                2
            )
        );

    if (
        roadsceneSelection
            .automaticResults()
            .size()
        != 2
    ) {
        return 6;
    }

    if (
        roadsceneSelection
            .automaticResults()
            .front()
            .pair()
            .sourceStem()
        != "FLIR_00001"
    ) {
        return 7;
    }

    if (
        roadsceneSelection
            .automaticResults()
            .back()
            .pair()
            .sourceStem()
        != "FLIR_00005"
    ) {
        return 8;
    }

    // ------------------------------------------------------------
    // Explicit Mode C order is preserved.
    // ------------------------------------------------------------

    std::vector<PairValidationResult>
        explicitCandidates;

    for (int index = 1; index <= 8; ++index) {
        explicitCandidates.push_back(
            validResult(
                "llvip",
                "train",
                "00000"
                    + std::to_string(
                        index
                    )
            )
        );
    }

    const auto explicitSelection =
        selector.select(
            explicitCandidates,
            PreviewSelectionRequest(
                2,
                {
                    "llvip_train_000006",
                    "llvip_train_000003"
                }
            )
        );

    if (
        explicitSelection
            .explicitResults()
            .size()
        != 2
    ) {
        return 9;
    }

    if (
        explicitSelection
            .explicitResults()
            .at(0)
            .pair()
            .pairId()
        != "llvip_train_000006"
    ) {
        return 10;
    }

    if (
        explicitSelection
            .explicitResults()
            .at(1)
            .pair()
            .pairId()
        != "llvip_train_000003"
    ) {
        return 11;
    }

    // ------------------------------------------------------------
    // Explicit ID already in B is not duplicated.
    // ------------------------------------------------------------

    std::vector<PairValidationResult>
        fiveResults;

    for (int index = 1; index <= 5; ++index) {
        fiveResults.push_back(
            validResult(
                "llvip",
                "train",
                "00000"
                    + std::to_string(
                        index
                    )
            )
        );
    }

    const auto overlapSelection =
        selector.select(
            fiveResults,
            PreviewSelectionRequest(
                5,
                {
                    "llvip_train_000003"
                }
            )
        );

    if (
        overlapSelection
            .automaticResults()
            .size()
        != 5
        || !overlapSelection
                .explicitResults()
                .empty()
        || overlapSelection
                .totalSelected()
            != 5
    ) {
        return 12;
    }

    // ------------------------------------------------------------
    // Repeated explicit IDs deduplicate in first-occurrence order.
    // ------------------------------------------------------------

    const auto deduplicated =
        selector.select(
            explicitCandidates,
            PreviewSelectionRequest(
                2,
                {
                    "llvip_train_000004",
                    "llvip_train_000004",
                    "llvip_train_000006",
                    "llvip_train_000004"
                }
            )
        );

    if (
        deduplicated
            .explicitResults()
            .size()
        != 2
    ) {
        return 13;
    }

    if (
        deduplicated
            .explicitResults()
            .at(0)
            .pair()
            .pairId()
        != "llvip_train_000004"
        || deduplicated
            .explicitResults()
            .at(1)
            .pair()
            .pairId()
        != "llvip_train_000006"
    ) {
        return 14;
    }

    // ------------------------------------------------------------
    // Missing explicit ID is a hard error.
    // ------------------------------------------------------------

    try {
        static_cast<void>(
            selector.select(
                lowCountResults,
                PreviewSelectionRequest(
                    5,
                    {
                        "llvip_test_999999"
                    }
                )
            )
        );

        return 15;
    }
    catch (
        const PreviewSelectionError& error
    ) {
        if (
            !containsText(
                error.what(),
                "missing IDs=[llvip_test_999999]"
            )
        ) {
            return 16;
        }
    }

    // ------------------------------------------------------------
    // Non-VALID explicit ID is a hard error.
    // ------------------------------------------------------------

    try {
        static_cast<void>(
            selector.select(
                mixedResults,
                PreviewSelectionRequest(
                    5,
                    {
                        "llvip_train_000002"
                    }
                )
            )
        );

        return 17;
    }
    catch (
        const PreviewSelectionError& error
    ) {
        if (
            !containsText(
                error.what(),
                "llvip_train_000002:MISSING_VISIBLE"
            )
        ) {
            return 18;
        }
    }

    // ------------------------------------------------------------
    // Duplicate candidate IDs are a hard input error.
    // ------------------------------------------------------------

    const auto duplicate =
        validResult(
            "llvip",
            "train",
            "000001"
        );

    try {
        static_cast<void>(
            selector.select(
                {
                    duplicate,
                    duplicate
                },
                PreviewSelectionRequest()
            )
        );

        return 19;
    }
    catch (
        const PreviewSelectionError& error
    ) {
        if (
            !containsText(
                error.what(),
                "duplicate pair ID"
            )
        ) {
            return 20;
        }
    }

    return 0;
}