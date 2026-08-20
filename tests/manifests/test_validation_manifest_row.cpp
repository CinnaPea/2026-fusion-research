//
// Created by hakgu on 8/18/2026.
//

#include "core/manifests/validation_manifest_row.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace {

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;

using qart::core::domain::imaging::ImageDimensions;

using qart::core::domain::validation::
    PairValidationResult;
using qart::core::domain::validation::
    PairValidationStatus;

using qart::core::manifests::
    ValidationManifestRow;
using qart::core::manifests::
    kValidationManifestSchemaVersion;

DatasetPair makeLlVipPair(
    const std::string& stem
)
{
    return DatasetPair(
        "llvip_train_" + stem,
        "llvip",
        DatasetPartitionId("train"),
        stem,
        "raw/llvip/visible/train/"
            + stem
            + ".jpg",
        "raw/llvip/infrared/train/"
            + stem
            + ".jpg"
    );
}

PairValidationResult validResult(
    const DatasetPair& pair
)
{
    const ImageDimensions dimensions(
        1280,
        1024
    );

    return PairValidationResult(
        pair,
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}

PairValidationResult missingVisibleResult(
    const DatasetPair& pair
)
{
    return PairValidationResult(
        pair,
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
    // Valid result -> canonical row
    // ------------------------------------------------------------

    const DatasetPair pair =
        makeLlVipPair(
            "010001"
        );

    const PairValidationResult result =
        validResult(
            pair
        );

    const ValidationManifestRow row =
        ValidationManifestRow::fromResult(
            result
        );

    if (
        row.pairId()
        != "llvip_train_010001"
    ) {
        return 1;
    }

    if (row.datasetId() != "llvip") {
        return 2;
    }

    if (row.partitionId() != "train") {
        return 3;
    }

    if (
        row.visibleRelativePath()
        != "raw/llvip/visible/train/010001.jpg"
    ) {
        return 4;
    }

    if (
        row.thermalRelativePath()
        != "raw/llvip/infrared/train/010001.jpg"
    ) {
        return 5;
    }

    if (row.status() != "VALID") {
        return 6;
    }

    if (
        !row.visibleDecoded()
        || !row.thermalDecoded()
    ) {
        return 7;
    }

    if (
        row.visibleWidth()
        != std::optional<int>(1280)
        || row.visibleHeight()
        != std::optional<int>(1024)
    ) {
        return 8;
    }

    // ------------------------------------------------------------
    // Typed round-trip
    // ------------------------------------------------------------

    if (row.toResult() != result) {
        return 9;
    }

    // ------------------------------------------------------------
    // Schema 2.0 CSV values
    // ------------------------------------------------------------

    const auto csvValues =
        row.toCsvValues();

    if (
        csvValues.at(0)
        != kValidationManifestSchemaVersion
    ) {
        return 10;
    }

    if (
        csvValues.at(3)
        != "train"
    ) {
        return 11;
    }

    if (
        csvValues.at(8)
        != "true"
        || csvValues.at(9)
        != "true"
    ) {
        return 12;
    }

    if (
        csvValues.at(10)
        != "1280"
        || csvValues.at(11)
        != "1024"
        || csvValues.at(12)
        != "1280"
        || csvValues.at(13)
        != "1024"
    ) {
        return 13;
    }

    // ------------------------------------------------------------
    // Missing visible dimensions serialize as empty fields
    // ------------------------------------------------------------

    const PairValidationResult
        missingResult =
            missingVisibleResult(
                makeLlVipPair(
                    "010002"
                )
            );

    const ValidationManifestRow
        missingRow =
            ValidationManifestRow::fromResult(
                missingResult
            );

    if (
        missingRow.visibleWidth().has_value()
        || missingRow.visibleHeight().has_value()
    ) {
        return 14;
    }

    const auto missingCsv =
        missingRow.toCsvValues();

    if (
        missingCsv.at(8) != "false"
        || missingCsv.at(9) != "true"
    ) {
        return 15;
    }

    if (
        !missingCsv.at(10).empty()
        || !missingCsv.at(11).empty()
    ) {
        return 16;
    }

    if (
        missingCsv.at(12) != "1280"
        || missingCsv.at(13) != "1024"
    ) {
        return 17;
    }

    if (
        missingRow.toResult()
        != missingResult
    ) {
        return 18;
    }

    // ------------------------------------------------------------
    // Native RoadScene "all" is canonical
    // ------------------------------------------------------------

    const DatasetPair roadscenePair(
        "roadscene_all_FLIR_00006",
        "roadscene",
        DatasetPartitionId("all"),
        "FLIR_00006",
        "raw/roadscene/crop_LR_visible/FLIR_00006.jpg",
        "raw/roadscene/cropinfrared/FLIR_00006.jpg"
    );

    const ImageDimensions roadsceneDimensions(
        640,
        512
    );

    const ValidationManifestRow roadsceneRow =
        ValidationManifestRow::fromResult(
            PairValidationResult(
                roadscenePair,
                PairValidationStatus::Valid,
                true,
                true,
                roadsceneDimensions,
                roadsceneDimensions
            )
        );

    if (
        roadsceneRow.partitionId()
        != "all"
    ) {
        return 19;
    }

    // ------------------------------------------------------------
    // Partial dimensions rejected
    // ------------------------------------------------------------

    try {
        const ValidationManifestRow invalid(
            "llvip_train_010003",
            "llvip",
            "train",
            "010003",
            "raw/llvip/visible/train/010003.jpg",
            "raw/llvip/infrared/train/010003.jpg",
            "VALID",
            true,
            true,
            1280,
            std::nullopt,
            1280,
            1024
        );

        static_cast<void>(invalid);

        return 20;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Nonpositive dimensions rejected
    // ------------------------------------------------------------

    try {
        const ValidationManifestRow invalid(
            "llvip_train_010004",
            "llvip",
            "train",
            "010004",
            "raw/llvip/visible/train/010004.jpg",
            "raw/llvip/infrared/train/010004.jpg",
            "DIMENSION_MISMATCH",
            true,
            true,
            1280,
            1024,
            0,
            1024
        );

        static_cast<void>(invalid);

        return 21;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Unknown status remains raw row data,
    // but typed reconstruction must reject it.
    // ------------------------------------------------------------

    const ValidationManifestRow unknownStatus(
        "llvip_train_010005",
        "llvip",
        "train",
        "010005",
        "raw/llvip/visible/train/010005.jpg",
        "raw/llvip/infrared/train/010005.jpg",
        "UNKNOWN_STATUS",
        true,
        true,
        1280,
        1024,
        1280,
        1024
    );

    try {
        static_cast<void>(
            unknownStatus.toResult()
        );

        return 22;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Invalid native partition rejected at row construction
    // ------------------------------------------------------------

    try {
        const ValidationManifestRow invalidPartition(
            "llvip_train_010006",
            "llvip",
            "Train",
            "010006",
            "raw/llvip/visible/train/010006.jpg",
            "raw/llvip/infrared/train/010006.jpg",
            "VALID",
            true,
            true,
            1280,
            1024,
            1280,
            1024
        );

        static_cast<void>(
            invalidPartition
        );

        return 23;
    }
    catch (const std::invalid_argument&) {
    }

    const ValidationManifestRow equivalent =
        ValidationManifestRow::fromResult(
            result
        );

    if (row != equivalent) {
        return 24;
    }

    return 0;
}