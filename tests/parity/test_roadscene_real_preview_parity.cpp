//
// Created by hakgu on 8/21/2026.
//

#include "core/manifests/csv_validation_manifest_reader.h"

#include "core/visualization/opencv_preview_renderer.h"
#include "core/visualization/preview_pair_selector.h"
#include "core/visualization/preview_renderer.h"
#include "core/visualization/preview_selection.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::core::manifests::
    CsvValidationManifestReader;

using qart::core::visualization::
    OpenCvPreviewRenderer;
using qart::core::visualization::
    PreviewPairSelector;
using qart::core::visualization::
    PreviewRenderRequest;
using qart::core::visualization::
    PreviewSelectionRequest;


class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
              std::filesystem::temp_directory_path()
              / "qart_roadscene_real_preview_parity"
          )
    {
        std::error_code error;

        std::filesystem::remove_all(
            path_,
            error
        );

        std::filesystem::create_directories(
            path_
        );
    }

    ~TemporaryDirectory()
    {
        std::error_code error;

        std::filesystem::remove_all(
            path_,
            error
        );
    }

    [[nodiscard]]
    const std::filesystem::path&
    path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};


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


cv::Mat toDisplayBgr(
    const cv::Mat& source
)
{
    if (source.empty()) {
        throw std::runtime_error(
            "Source image is empty."
        );
    }

    if (source.depth() != CV_8U) {
        throw std::runtime_error(
            "RoadScene source is not 8-bit."
        );
    }

    cv::Mat display;

    switch (source.channels()) {
        case 1:
            cv::cvtColor(
                source,
                display,
                cv::COLOR_GRAY2BGR
            );
            break;

        case 3:
            display =
                source.clone();
            break;

        case 4:
            cv::cvtColor(
                source,
                display,
                cv::COLOR_BGRA2BGR
            );
            break;

        default:
            throw std::runtime_error(
                "Unsupported RoadScene source channel count: channels="
                + std::to_string(
                    source.channels()
                )
                + ", type="
                + std::to_string(
                    source.type()
                )
                + ", depth="
                + std::to_string(
                    source.depth()
                )
            );
    }

    return display;
}


bool matricesEqual(
    const cv::Mat& left,
    const cv::Mat& right
)
{
    if (
        left.rows != right.rows
        || left.cols != right.cols
        || left.type() != right.type()
    ) {
        return false;
    }

    cv::Mat difference;

    cv::compare(
        left,
        right,
        difference,
        cv::CMP_NE
    );

    return
        cv::countNonZero(
            difference.reshape(1)
        )
        == 0;
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
            "Usage: test_roadscene_real_preview_parity "
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
        !std::filesystem::is_regular_file(
            manifestPath
        )
    ) {
        return fail(
            2,
            "Frozen RoadScene Schema 2.0 manifest "
            "was not found: "
            + manifestPath.string()
        );
    }


    // ------------------------------------------------------------
    // 1. Read frozen Python-produced manifest.
    // ------------------------------------------------------------

    CsvValidationManifestReader reader;

    const auto manifest =
        reader.read(
            manifestPath
        );

    const auto results =
        manifest.toResults();


    // ------------------------------------------------------------
    // 2. Reproduce frozen Mode B selection.
    // ------------------------------------------------------------

    PreviewPairSelector selector;

    const auto selection =
        selector.select(
            results,
            PreviewSelectionRequest(
                5
            )
        );

    const auto& selected =
        selection.automaticResults();

    const std::vector<std::string>
        expectedPairIds = {
            "roadscene_all_FLIR_00006",
            "roadscene_all_FLIR_04726",
            "roadscene_all_FLIR_06660",
            "roadscene_all_FLIR_08220",
            "roadscene_all_FLIR_video_04215"
        };

    if (
        selected.size()
        != expectedPairIds.size()
    ) {
        return fail(
            3,
            "Expected exactly five frozen "
            "RoadScene Mode B pairs."
        );
    }


    TemporaryDirectory temporary;

    const std::filesystem::path outputRoot =
        temporary.path()
        / "previews";

    OpenCvPreviewRenderer renderer;


    // ------------------------------------------------------------
    // 3. Render every real RoadScene Mode B pair.
    // ------------------------------------------------------------

    for (
        std::size_t index = 0;
        index < selected.size();
        ++index
    ) {
        const auto& validationResult =
            selected.at(
                index
            );

        const std::string& pairId =
            validationResult
                .pair()
                .pairId();

        if (
            pairId
            != expectedPairIds.at(
                index
            )
        ) {
            return fail(
                4,
                "Unexpected RoadScene Mode B identity "
                "at index "
                + std::to_string(index)
                + ": "
                + pairId
            );
        }


        // --------------------------------------------------------
        // RoadScene native partition must remain "all".
        // --------------------------------------------------------

        if (
            validationResult
                .pair()
                .partitionId()
                .value()
            != "all"
        ) {
            return fail(
                5,
                "RoadScene Mode B pair lost native "
                "partition 'all': "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Frozen validation result must provide pair-specific
        // dimensions for both modalities.
        // --------------------------------------------------------

        if (
            !validationResult
                .visibleDimensions()
                .has_value()
            || !validationResult
                .thermalDimensions()
                .has_value()
        ) {
            return fail(
                6,
                "RoadScene Mode B pair lacks dimensions: "
                + pairId
            );
        }

        const auto& visibleDimensions =
            validationResult
                .visibleDimensions()
                .value();

        const auto& thermalDimensions =
            validationResult
                .thermalDimensions()
                .value();

        if (
            visibleDimensions
            != thermalDimensions
        ) {
            return fail(
                7,
                "RoadScene Mode B pair has mismatching "
                "visible/thermal dimensions: "
                + pairId
            );
        }

        const int width =
            visibleDimensions.width();

        const int height =
            visibleDimensions.height();

        if (
            width <= 0
            || height <= 0
        ) {
            return fail(
                8,
                "RoadScene Mode B pair has non-positive "
                "dimensions: "
                + pairId
            );
        }


        const PreviewRenderRequest request(
            validationResult,
            datasetRoot,
            outputRoot
        );


        // --------------------------------------------------------
        // Canonical RoadScene crop sources are JPG.
        // --------------------------------------------------------

        if (
            request
                .visibleSourcePath()
                .extension()
            != ".jpg"
            || request
                .thermalSourcePath()
                .extension()
            != ".jpg"
        ) {
            return fail(
                9,
                "RoadScene canonical preview source "
                "is not JPG: "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Independently decode canonical source images.
        // --------------------------------------------------------

        const cv::Mat rawVisible =
            cv::imread(
                request
                    .visibleSourcePath()
                    .string(),
                cv::IMREAD_UNCHANGED
            );

        const cv::Mat rawThermal =
            cv::imread(
                request
                    .thermalSourcePath()
                    .string(),
                cv::IMREAD_UNCHANGED
            );

        if (
            rawVisible.empty()
            || rawThermal.empty()
        ) {
            return fail(
                10,
                "Independent OpenCV source decode failed for "
                + pairId
            );
        }

        cv::Mat expectedVisible;
        cv::Mat expectedThermal;

        try {
            expectedVisible =
                toDisplayBgr(
                    rawVisible
                );

            expectedThermal =
                toDisplayBgr(
                    rawThermal
                );
        }
        catch (
            const std::exception& error
        ) {
            return fail(
                11,
                pairId
                + ": "
                + error.what()
            );
        }


        // --------------------------------------------------------
        // Independent decode must agree with frozen manifest
        // geometry.
        // --------------------------------------------------------

        if (
            expectedVisible.cols != width
            || expectedVisible.rows != height
            || expectedThermal.cols != width
            || expectedThermal.rows != height
        ) {
            return fail(
                12,
                "RoadScene real image geometry differs "
                "from frozen manifest for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Production renderer.
        // --------------------------------------------------------

        const auto renderResult =
            renderer.render(
                request
            );

        if (
            renderResult.pairId()
            != pairId
        ) {
            return fail(
                13,
                "Renderer returned wrong pair ID for "
                + pairId
            );
        }

        if (
            !std::filesystem::is_regular_file(
                renderResult.outputPath()
            )
        ) {
            return fail(
                14,
                "Preview PNG was not written for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Independently decode persisted preview.
        // --------------------------------------------------------

        const cv::Mat preview =
            cv::imread(
                renderResult
                    .outputPath()
                    .string(),
                cv::IMREAD_UNCHANGED
            );

        if (preview.empty()) {
            return fail(
                15,
                "Could not decode rendered RoadScene preview for "
                + pairId
            );
        }

        const int headerHeight =
            renderResult.headerHeight();

        if (headerHeight <= 0) {
            return fail(
                16,
                "Non-positive preview header for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Pair-specific geometry.
        //
        // No globally fixed RoadScene dimensions are allowed.
        // --------------------------------------------------------

        if (
            preview.cols
                != width * 2
            || preview.rows
                != height + headerHeight
            || preview.channels()
                != 3
        ) {
            return fail(
                17,
                "RoadScene preview geometry mismatch for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Exact visible and thermal body regions.
        // --------------------------------------------------------

        const cv::Mat renderedVisible =
            preview(
                cv::Rect(
                    0,
                    headerHeight,
                    width,
                    height
                )
            );

        const cv::Mat renderedThermal =
            preview(
                cv::Rect(
                    width,
                    headerHeight,
                    width,
                    height
                )
            );


        // --------------------------------------------------------
        // Pixel-for-pixel preservation.
        // --------------------------------------------------------

        if (
            !matricesEqual(
                renderedVisible,
                expectedVisible
            )
        ) {
            return fail(
                18,
                "Visible body pixels changed for "
                + pairId
            );
        }

        if (
            !matricesEqual(
                renderedThermal,
                expectedThermal
            )
        ) {
            return fail(
                19,
                "Thermal body pixels changed for "
                + pairId
            );
        }


        std::cout
            << "[PASS] "
            << pairId
            << " | source="
            << width
            << 'x'
            << height
            << " | visible_channels="
            << rawVisible.channels()
            << " | thermal_channels="
            << rawThermal.channels()
            << " | preview="
            << preview.cols
            << 'x'
            << preview.rows
            << " | header="
            << headerHeight
            << '\n';
    }


    std::cout
        << "[PASS] RoadScene real-image preview parity\n"
        << "  Mode B pairs: 5\n"
        << "  Native partition: all\n"
        << "  Pair-specific geometry: preserved\n"
        << "  Variable-resolution rendering: verified\n"
        << "  Visible body pixels: exact\n"
        << "  Thermal body pixels: exact\n"
        << "  Left/right placement: exact\n"
        << "  No resize/enhancement/registration/fusion\n"
        << "  Pixel-perfect registration claim: NOT made\n";

    return 0;
}