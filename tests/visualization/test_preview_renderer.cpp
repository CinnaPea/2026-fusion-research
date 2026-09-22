//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/preview_renderer.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>

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
    PreviewRenderer;
using qart::core::visualization::
    PreviewRenderRequest;
using qart::core::visualization::
    PreviewRenderResult;


PairValidationResult makeValidLlVipResult()
{
    const ImageDimensions dimensions(
        1280,
        1024
    );

    return PairValidationResult(
        DatasetPair(
            "llvip_train_010001",
            "llvip",
            DatasetPartitionId("train"),
            "010001",
            "raw/llvip/visible/train/010001.jpg",
            "raw/llvip/infrared/train/010001.jpg"
        ),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}


PairValidationResult makeInvalidLlVipResult()
{
    return PairValidationResult(
        DatasetPair(
            "llvip_train_010001",
            "llvip",
            DatasetPartitionId("train"),
            "010001",
            "raw/llvip/visible/train/010001.jpg",
            "raw/llvip/infrared/train/010001.jpg"
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
            "Injected invalid result."
        )
    );
}


PairValidationResult makeRoadSceneResult()
{
    const ImageDimensions dimensions(
        640,
        512
    );

    return PairValidationResult(
        DatasetPair(
            "roadscene_all_FLIR_00006",
            "roadscene",
            DatasetPartitionId("all"),
            "FLIR_00006",
            "raw/roadscene/crop_LR_visible/FLIR_00006.jpg",
            "raw/roadscene/cropinfrared/FLIR_00006.jpg"
        ),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}

} // namespace


int main()
{
    static_assert(
        std::is_abstract_v<
            PreviewRenderer
        >
    );

    const auto absoluteRoot =
        std::filesystem::absolute(
            std::filesystem::path(
                "preview-renderer-contract-root"
            )
        );

    const auto outputRoot =
        absoluteRoot
        / "derived"
        / "previews";

    // ------------------------------------------------------------
    // Valid request.
    // ------------------------------------------------------------

    const PreviewRenderRequest request(
        makeValidLlVipResult(),
        absoluteRoot,
        outputRoot
    );

    if (
        request.overwrite()
    ) {
        return 1;
    }

    if (
        request.visibleSourcePath()
        != absoluteRoot
            / "raw"
            / "llvip"
            / "visible"
            / "train"
            / "010001.jpg"
    ) {
        return 2;
    }

    if (
        request.thermalSourcePath()
        != absoluteRoot
            / "raw"
            / "llvip"
            / "infrared"
            / "train"
            / "010001.jpg"
    ) {
        return 3;
    }

    if (
        request.outputPath()
        != outputRoot
            / "llvip"
            / "train"
            / "llvip_train_010001.png"
    ) {
        return 4;
    }

    // ------------------------------------------------------------
    // RoadScene proves canonical native partition output.
    // ------------------------------------------------------------

    const PreviewRenderRequest roadsceneRequest(
        makeRoadSceneResult(),
        absoluteRoot,
        outputRoot
    );

    if (
        roadsceneRequest.outputPath()
        != outputRoot
            / "roadscene"
            / "all"
            / "roadscene_all_FLIR_00006.png"
    ) {
        return 5;
    }

    // ------------------------------------------------------------
    // Non-VALID result rejected.
    // ------------------------------------------------------------

    try {
        const PreviewRenderRequest invalid(
            makeInvalidLlVipResult(),
            absoluteRoot,
            outputRoot
        );

        static_cast<void>(
            invalid
        );

        return 6;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Relative dataset root rejected.
    // ------------------------------------------------------------

    try {
        const PreviewRenderRequest invalid(
            makeValidLlVipResult(),
            std::filesystem::path(
                "relative-dataset"
            ),
            outputRoot
        );

        static_cast<void>(
            invalid
        );

        return 7;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Relative output root rejected.
    // ------------------------------------------------------------

    try {
        const PreviewRenderRequest invalid(
            makeValidLlVipResult(),
            absoluteRoot,
            std::filesystem::path(
                "relative-output"
            )
        );

        static_cast<void>(
            invalid
        );

        return 8;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Valid result geometry.
    // ------------------------------------------------------------

    const PreviewRenderResult result(
        "llvip_train_010001",
        outputRoot
            / "llvip"
            / "train"
            / "llvip_train_010001.png",
        ImageDimensions(
            1280,
            1024
        ),
        ImageDimensions(
            2560,
            1104
        ),
        80
    );

    if (
        result.headerHeight()
        != 80
    ) {
        return 9;
    }

    if (
        result.previewDimensions()
            != ImageDimensions(
                2560,
                1104
            )
    ) {
        return 10;
    }

    // ------------------------------------------------------------
    // Width must be exactly 2 × source width.
    // ------------------------------------------------------------

    try {
        const PreviewRenderResult invalid(
            "llvip_train_010001",
            outputRoot
                / "llvip"
                / "train"
                / "llvip_train_010001.png",
            ImageDimensions(
                1280,
                1024
            ),
            ImageDimensions(
                1280,
                1104
            ),
            80
        );

        static_cast<void>(
            invalid
        );

        return 11;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Height must be source height + header.
    // ------------------------------------------------------------

    try {
        const PreviewRenderResult invalid(
            "llvip_train_010001",
            outputRoot
                / "llvip"
                / "train"
                / "llvip_train_010001.png",
            ImageDimensions(
                1280,
                1024
            ),
            ImageDimensions(
                2560,
                1024
            ),
            80
        );

        static_cast<void>(
            invalid
        );

        return 12;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Header height must be positive.
    // ------------------------------------------------------------

    try {
        const PreviewRenderResult invalid(
            "llvip_train_010001",
            outputRoot
                / "llvip"
                / "train"
                / "llvip_train_010001.png",
            ImageDimensions(
                1280,
                1024
            ),
            ImageDimensions(
                2560,
                1024
            ),
            0
        );

        static_cast<void>(
            invalid
        );

        return 13;
    }
    catch (const std::invalid_argument&) {
    }

    return 0;
}