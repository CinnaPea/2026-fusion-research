//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/validation/pair_validation_result.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace {

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::imaging::ImageDimensions;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;

DatasetPair makePair()
{
    return DatasetPair(
        "llvip_train_010001",
        "llvip",
        DatasetPartitionId("train"),
        "010001",
        "raw/llvip/visible/train/010001.jpg",
        "raw/llvip/infrared/train/010001.jpg"
    );
}

ImageDimensions llvipDimensions()
{
    return ImageDimensions(
        1280,
        1024
    );
}

bool rejectsVisibleStateMismatch(
    const bool decoded,
    const std::optional<ImageDimensions>& dimensions
)
{
    try {
        const PairValidationResult result(
            makePair(),
            PairValidationStatus::MissingVisible,
            decoded,
            true,
            dimensions,
            llvipDimensions()
        );

        static_cast<void>(result);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsThermalStateMismatch(
    const bool decoded,
    const std::optional<ImageDimensions>& dimensions
)
{
    try {
        const PairValidationResult result(
            makePair(),
            PairValidationStatus::MissingThermal,
            true,
            decoded,
            llvipDimensions(),
            dimensions
        );

        static_cast<void>(result);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsValidWithoutBothDecoded()
{
    try {
        const PairValidationResult result(
            makePair(),
            PairValidationStatus::Valid,
            false,
            true,
            std::nullopt,
            llvipDimensions()
        );

        static_cast<void>(result);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsValidDimensionMismatch()
{
    try {
        const PairValidationResult result(
            makePair(),
            PairValidationStatus::Valid,
            true,
            true,
            ImageDimensions(1280, 1024),
            ImageDimensions(640, 480)
        );

        static_cast<void>(result);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const ImageDimensions dimensions =
        llvipDimensions();

    const PairValidationResult validResult(
        makePair(),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );

    if (!validResult.isValid()) {
        return 1;
    }

    if (!validResult.dimensionsMatch()) {
        return 2;
    }

    if (!validResult.visibleDecoded()) {
        return 3;
    }

    if (!validResult.thermalDecoded()) {
        return 4;
    }

    if (!validResult.visibleDimensions().has_value()) {
        return 5;
    }

    if (!validResult.thermalDimensions().has_value()) {
        return 6;
    }

    const PairValidationResult missingVisible(
        makePair(),
        PairValidationStatus::MissingVisible,
        false,
        true,
        std::nullopt,
        dimensions,
        std::string(
            "Visible source file was not found."
        )
    );

    if (missingVisible.isValid()) {
        return 7;
    }

    if (missingVisible.dimensionsMatch()) {
        return 8;
    }

    if (missingVisible.visibleDimensions().has_value()) {
        return 9;
    }

    if (!missingVisible.thermalDimensions().has_value()) {
        return 10;
    }

    if (!missingVisible.message().has_value()) {
        return 11;
    }

    const PairValidationResult mismatch(
        makePair(),
        PairValidationStatus::DimensionMismatch,
        true,
        true,
        ImageDimensions(1280, 1024),
        ImageDimensions(640, 480)
    );

    if (mismatch.isValid()) {
        return 12;
    }

    if (mismatch.dimensionsMatch()) {
        return 13;
    }

    if (
        !rejectsVisibleStateMismatch(
            true,
            std::nullopt
        )
    ) {
        return 14;
    }

    if (
        !rejectsVisibleStateMismatch(
            false,
            dimensions
        )
    ) {
        return 15;
    }

    if (
        !rejectsThermalStateMismatch(
            true,
            std::nullopt
        )
    ) {
        return 16;
    }

    if (
        !rejectsThermalStateMismatch(
            false,
            dimensions
        )
    ) {
        return 17;
    }

    if (!rejectsValidWithoutBothDecoded()) {
        return 18;
    }

    if (!rejectsValidDimensionMismatch()) {
        return 19;
    }

    const PairValidationResult equivalent(
        makePair(),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );

    if (validResult != equivalent) {
        return 20;
    }

    return 0;
}
