//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/preview_selection.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <optional>
#include <stdexcept>
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
    PreviewSelection;
using qart::core::visualization::
    PreviewSelectionRequest;


PairValidationResult makeValidResult(
    const std::string& pairId,
    const std::string& datasetId,
    const std::string& partitionId,
    const std::string& stem
)
{
    const ImageDimensions dimensions(
        640,
        512
    );

    return PairValidationResult(
        DatasetPair(
            pairId,
            datasetId,
            DatasetPartitionId(
                partitionId
            ),
            stem,
            "raw/"
                + datasetId
                + "/visible/"
                + stem
                + ".jpg",
            "raw/"
                + datasetId
                + "/thermal/"
                + stem
                + ".jpg"
        ),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}

PairValidationResult makeInvalidResult()
{
    return PairValidationResult(
        DatasetPair(
            "llvip_train_000002",
            "llvip",
            DatasetPartitionId(
                "train"
            ),
            "000002",
            "raw/llvip/visible/train/000002.jpg",
            "raw/llvip/infrared/train/000002.jpg"
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

} // namespace


int main()
{
    // ------------------------------------------------------------
    // Default request.
    // ------------------------------------------------------------

    const PreviewSelectionRequest defaultRequest;

    if (
        defaultRequest
            .automaticCountPerPartition()
        != 5
    ) {
        return 1;
    }

    if (
        !defaultRequest
            .explicitPairIds()
            .empty()
    ) {
        return 2;
    }

    // ------------------------------------------------------------
    // Explicit request.
    // ------------------------------------------------------------

    const PreviewSelectionRequest request(
        3,
        {
            "llvip_train_000001",
            "roadscene_all_FLIR_00006"
        }
    );

    if (
        request
            .automaticCountPerPartition()
        != 3
    ) {
        return 3;
    }

    if (
        request.explicitPairIds().size()
        != 2
    ) {
        return 4;
    }

    // ------------------------------------------------------------
    // Count below two rejected.
    // ------------------------------------------------------------

    try {
        const PreviewSelectionRequest invalid(
            1
        );

        static_cast<void>(
            invalid
        );

        return 5;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Blank explicit ID rejected.
    // ------------------------------------------------------------

    try {
        const PreviewSelectionRequest invalid(
            5,
            {
                "   "
            }
        );

        static_cast<void>(
            invalid
        );

        return 6;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Surrounding whitespace rejected.
    // ------------------------------------------------------------

    try {
        const PreviewSelectionRequest invalid(
            5,
            {
                " llvip_train_000001"
            }
        );

        static_cast<void>(
            invalid
        );

        return 7;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Automatic + explicit ordering.
    // RoadScene "all" proves no train/test dependency here.
    // ------------------------------------------------------------

    const auto automaticFirst =
        makeValidResult(
            "llvip_train_000001",
            "llvip",
            "train",
            "000001"
        );

    const auto automaticSecond =
        makeValidResult(
            "roadscene_all_FLIR_00006",
            "roadscene",
            "all",
            "FLIR_00006"
        );

    const auto explicitResult =
        makeValidResult(
            "llvip_test_190001",
            "llvip",
            "test",
            "190001"
        );

    const PreviewSelection selection(
        {
            automaticFirst,
            automaticSecond
        },
        {
            explicitResult
        }
    );

    if (
        selection
            .automaticResults()
            .size()
        != 2
    ) {
        return 8;
    }

    if (
        selection
            .explicitResults()
            .size()
        != 1
    ) {
        return 9;
    }

    if (
        selection.totalSelected()
        != 3
    ) {
        return 10;
    }

    const auto allResults =
        selection.allResults();

    if (
        allResults.at(0)
            .pair()
            .pairId()
        != "llvip_train_000001"
    ) {
        return 11;
    }

    if (
        allResults.at(1)
            .pair()
            .pairId()
        != "roadscene_all_FLIR_00006"
    ) {
        return 12;
    }

    if (
        allResults.at(2)
            .pair()
            .pairId()
        != "llvip_test_190001"
    ) {
        return 13;
    }

    // ------------------------------------------------------------
    // Non-VALID result forbidden.
    // ------------------------------------------------------------

    try {
        const PreviewSelection invalidSelection(
            {
                makeInvalidResult()
            },
            {}
        );

        static_cast<void>(
            invalidSelection
        );

        return 14;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Duplicate IDs across B and C forbidden.
    // ------------------------------------------------------------

    try {
        const PreviewSelection duplicateSelection(
            {
                automaticFirst
            },
            {
                automaticFirst
            }
        );

        static_cast<void>(
            duplicateSelection
        );

        return 15;
    }
    catch (const std::invalid_argument&) {
    }

    return 0;
}