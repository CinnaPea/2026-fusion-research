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
              / "qart_msrs_real_preview_parity"
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
            "MSRS source is not 8-bit."
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
                "Unsupported MSRS source channel count: channels="
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
            "Usage: test_msrs_real_preview_parity "
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
        !std::filesystem::is_regular_file(
            manifestPath
        )
    ) {
        return fail(
            2,
            "Frozen MSRS Schema 2.0 manifest not found: "
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
    // 2. Reproduce canonical Mode B.
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

    if (
        selected.size()
        != expectedPairIds.size()
    ) {
        return fail(
            3,
            "Expected exactly ten frozen "
            "MSRS Mode B pairs."
        );
    }


    TemporaryDirectory temporary;

    const auto outputRoot =
        temporary.path()
        / "previews";

    OpenCvPreviewRenderer renderer;


    // ------------------------------------------------------------
    // 3. Render each real Mode B pair.
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
                "Unexpected MSRS Mode B identity at index "
                + std::to_string(index)
                + ": "
                + pairId
            );
        }


        // --------------------------------------------------------
        // D/N suffix remains part of real identity.
        // --------------------------------------------------------

        const std::string& sourceStem =
            validationResult
                .pair()
                .sourceStem();

        if (
            sourceStem.empty()
            || (
                sourceStem.back() != 'D'
                && sourceStem.back() != 'N'
            )
        ) {
            return fail(
                5,
                "MSRS Mode B source stem lost D/N identity: "
                + sourceStem
            );
        }


        const PreviewRenderRequest request(
            validationResult,
            datasetRoot,
            outputRoot
        );


        // --------------------------------------------------------
        // Sources must remain canonical PNGs.
        // --------------------------------------------------------

        if (
            request
                .visibleSourcePath()
                .extension()
            != ".png"
            || request
                .thermalSourcePath()
                .extension()
            != ".png"
        ) {
            return fail(
                6,
                "MSRS canonical preview source is not PNG: "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Independently decode the actual source files.
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
                7,
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
                8,
                pairId
                + ": "
                + error.what()
            );
        }


        // --------------------------------------------------------
        // Frozen MSRS dimensions.
        // --------------------------------------------------------

        if (
            expectedVisible.cols != 640
            || expectedVisible.rows != 480
            || expectedThermal.cols != 640
            || expectedThermal.rows != 480
        ) {
            return fail(
                9,
                "Unexpected real MSRS geometry for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Production C++ rendering.
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
                10,
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
                11,
                "Preview PNG was not written for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Read the persisted preview.
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
                12,
                "Could not decode rendered MSRS preview for "
                + pairId
            );
        }

        const int headerHeight =
            renderResult.headerHeight();

        if (headerHeight <= 0) {
            return fail(
                13,
                "Non-positive preview header for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Preview geometry:
        //
        // 640 visible + 640 thermal = 1280 width.
        // 480 image body + header.
        // --------------------------------------------------------

        if (
            preview.cols != 1280
            || preview.rows
                != 480 + headerHeight
            || preview.channels() != 3
        ) {
            return fail(
                14,
                "MSRS preview geometry mismatch for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Exact source-body regions.
        // --------------------------------------------------------

        const cv::Mat renderedVisible =
            preview(
                cv::Rect(
                    0,
                    headerHeight,
                    640,
                    480
                )
            );

        const cv::Mat renderedThermal =
            preview(
                cv::Rect(
                    640,
                    headerHeight,
                    640,
                    480
                )
            );


        // --------------------------------------------------------
        // Pixel-for-pixel body parity.
        // --------------------------------------------------------

        if (
            !matricesEqual(
                renderedVisible,
                expectedVisible
            )
        ) {
            return fail(
                15,
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
                16,
                "Thermal body pixels changed for "
                + pairId
            );
        }


        std::cout
            << "[PASS] "
            << pairId
            << " | condition="
            << sourceStem.back()
            << " | visible_channels="
            << rawVisible.channels()
            << " | thermal_channels="
            << rawThermal.channels()
            << " | source=640x480"
            << " | preview=1280x"
            << preview.rows
            << " | header="
            << headerHeight
            << '\n';
    }


    std::cout
        << "[PASS] MSRS real-image preview parity\n"
        << "  Mode B pairs: 10\n"
        << "  Day/night identities: preserved\n"
        << "  Real PNG decoding: exact\n"
        << "  Visible body pixels: exact\n"
        << "  Thermal body pixels: exact\n"
        << "  Left/right placement: exact\n"
        << "  No resize/enhancement/registration/fusion\n";

    return 0;
}