//
// Created by hakgu on 8/20/2026.
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
              / "qart_llvip_real_preview_parity"
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
            "Expected 8-bit LLVIP source."
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
            display = source.clone();
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
                "Unsupported LLVIP source channel count: channels="
                + std::to_string(source.channels())
                + ", type="
                + std::to_string(source.type())
                + ", depth="
                + std::to_string(source.depth())
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
            "Usage: test_llvip_real_preview_parity "
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
        / "llvip"
        / "llvip_validation_schema_2_0.csv";

    if (
        !std::filesystem::is_regular_file(
            manifestPath
        )
    ) {
        return fail(
            2,
            "Frozen LLVIP Schema 2.0 manifest not found."
        );
    }


    // ------------------------------------------------------------
    // Read the Python-produced frozen manifest.
    // ------------------------------------------------------------

    CsvValidationManifestReader reader;

    const auto manifest =
        reader.read(
            manifestPath
        );

    const auto results =
        manifest.toResults();


    // ------------------------------------------------------------
    // Reproduce frozen Mode B in C++.
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
            "llvip_train_010001",
            "llvip_train_070021",
            "llvip_train_091058",
            "llvip_train_140063",
            "llvip_train_250423",

            "llvip_test_190001",
            "llvip_test_200039",
            "llvip_test_220150",
            "llvip_test_240167",
            "llvip_test_260536"
        };

    if (
        selected.size()
        != expectedPairIds.size()
    ) {
        return fail(
            3,
            "Expected exactly ten frozen Mode B pairs."
        );
    }


    TemporaryDirectory temporary;

    const auto outputRoot =
        temporary.path()
        / "previews";

    OpenCvPreviewRenderer renderer;


    // ------------------------------------------------------------
    // Render and inspect all ten REAL images.
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
                "Unexpected Mode B identity at index "
                + std::to_string(index)
                + ": "
                + pairId
            );
        }

        const PreviewRenderRequest request(
            validationResult,
            datasetRoot,
            outputRoot
        );


        // --------------------------------------------------------
        // Independently decode real sources for comparison.
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
                5,
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
                6,
                pairId
                + ": "
                + error.what()
            );
        }


        // --------------------------------------------------------
        // Frozen LLVIP geometry.
        // --------------------------------------------------------

        if (
            expectedVisible.cols != 1280
            || expectedVisible.rows != 1024
            || expectedThermal.cols != 1280
            || expectedThermal.rows != 1024
        ) {
            return fail(
                7,
                "Unexpected real LLVIP geometry for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // C++ production render.
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
                8,
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
                9,
                "Preview PNG was not written for "
                + pairId
            );
        }

        const cv::Mat preview =
            cv::imread(
                renderResult
                    .outputPath()
                    .string(),
                cv::IMREAD_UNCHANGED
            );

        if (preview.empty()) {
            return fail(
                10,
                "Could not decode rendered preview for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Preview geometry.
        // --------------------------------------------------------

        const int headerHeight =
            renderResult.headerHeight();

        if (headerHeight <= 0) {
            return fail(
                11,
                "Non-positive preview header for "
                + pairId
            );
        }

        if (
            preview.cols != 2560
            || preview.rows
                != 1024 + headerHeight
            || preview.channels() != 3
        ) {
            return fail(
                12,
                "Preview geometry mismatch for "
                + pairId
            );
        }


        // --------------------------------------------------------
        // Body begins AFTER the header.
        //
        // Left  = visible.
        // Right = thermal.
        // --------------------------------------------------------

        const cv::Mat renderedVisible =
            preview(
                cv::Rect(
                    0,
                    headerHeight,
                    1280,
                    1024
                )
            );

        const cv::Mat renderedThermal =
            preview(
                cv::Rect(
                    1280,
                    headerHeight,
                    1280,
                    1024
                )
            );


        // --------------------------------------------------------
        // Exact body-pixel parity.
        // --------------------------------------------------------

        if (
            !matricesEqual(
                renderedVisible,
                expectedVisible
            )
        ) {
            return fail(
                13,
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
                14,
                "Thermal body pixels changed for "
                + pairId
            );
        }

        std::cout
            << "[PASS] "
            << pairId
            << " | source=1280x1024"
            << " | preview=2560x"
            << preview.rows
            << " | header="
            << headerHeight
            << '\n';
    }


    std::cout
        << "[PASS] LLVIP real-image preview parity\n"
        << "  Mode B pairs: 10\n"
        << "  Real source decoding: exact\n"
        << "  Visible body pixels: exact\n"
        << "  Thermal body pixels: exact\n"
        << "  Left/right placement: exact\n"
        << "  No resize/enhancement/registration/fusion\n";

    return 0;
}