//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/opencv_preview_renderer.h"

#include "core/domain/imaging/image_dimensions.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace qart::core::visualization {

namespace {

using domain::imaging::ImageDimensions;

constexpr int kFontFace =
    cv::FONT_HERSHEY_SIMPLEX;

constexpr double kFontScale =
    0.7;

constexpr int kFontThickness =
    2;

constexpr int kTextPadding =
    12;

constexpr int kLineGap =
    10;


cv::Mat loadDisplayImage(
    const std::filesystem::path& imagePath,
    const std::string& modalityName
)
{
    if (
        !std::filesystem::is_regular_file(
            imagePath
        )
    ) {
        throw PreviewRenderError(
            modalityName
            + " preview source does not exist: "
            + imagePath.string()
        );
    }

    cv::Mat decoded;

    try {
        decoded = cv::imread(
            imagePath.string(),
            cv::IMREAD_UNCHANGED
        );
    }
    catch (const cv::Exception&) {
        throw PreviewRenderError(
            "OpenCV failed while reading "
            + modalityName
            + " source: "
            + imagePath.string()
        );
    }

    if (decoded.empty()) {
        throw PreviewRenderError(
            "OpenCV could not decode "
            + modalityName
            + " source: "
            + imagePath.string()
        );
    }

    if (
        decoded.depth()
        != CV_8U
    ) {
        throw PreviewRenderError(
            modalityName
            + " preview source must decode as "
              "8-bit data."
        );
    }

    cv::Mat displayImage;

    switch (decoded.channels()) {
        case 1:
            cv::cvtColor(
                decoded,
                displayImage,
                cv::COLOR_GRAY2BGR
            );
            break;

        case 3:
            displayImage =
                decoded.isContinuous()
                ? decoded
                : decoded.clone();
            break;

        case 4:
            cv::cvtColor(
                decoded,
                displayImage,
                cv::COLOR_BGRA2BGR
            );
            break;

        default:
            throw PreviewRenderError(
                modalityName
                + " preview source has unsupported "
                  "channel count: "
                + std::to_string(
                    decoded.channels()
                )
                + "."
            );
    }

    if (
        displayImage.rows <= 0
        || displayImage.cols <= 0
    ) {
        throw PreviewRenderError(
            modalityName
            + " preview source has invalid "
              "spatial dimensions."
        );
    }

    if (!displayImage.isContinuous()) {
        displayImage =
            displayImage.clone();
    }

    return displayImage;
}


void validateLoadedDimensions(
    const cv::Mat& image,
    const ImageDimensions& expectedDimensions,
    const std::string& modalityName,
    const std::filesystem::path& imagePath
)
{
    const ImageDimensions
        observedDimensions(
            image.cols,
            image.rows
        );

    if (
        observedDimensions
        != expectedDimensions
    ) {
        throw PreviewRenderError(
            modalityName
            + " preview dimensions changed after "
              "manifest validation: expected="
            + std::to_string(
                expectedDimensions.width()
            )
            + "x"
            + std::to_string(
                expectedDimensions.height()
            )
            + ", observed="
            + std::to_string(
                observedDimensions.width()
            )
            + "x"
            + std::to_string(
                observedDimensions.height()
            )
            + ", path="
            + imagePath.string()
        );
    }
}


cv::Mat buildHeader(
    const int sourceWidth,
    const std::string& pairId
)
{
    const std::string pairText =
        pairId;

    const std::string visibleText =
        "VISIBLE";

    const std::string thermalText =
        "THERMAL / INFRARED";

    int pairBaseline = 0;
    int visibleBaseline = 0;
    int thermalBaseline = 0;

    const cv::Size pairSize =
        cv::getTextSize(
            pairText,
            kFontFace,
            kFontScale,
            kFontThickness,
            &pairBaseline
        );

    const cv::Size visibleSize =
        cv::getTextSize(
            visibleText,
            kFontFace,
            kFontScale,
            kFontThickness,
            &visibleBaseline
        );

    const cv::Size thermalSize =
        cv::getTextSize(
            thermalText,
            kFontFace,
            kFontScale,
            kFontThickness,
            &thermalBaseline
        );

    const int firstLineHeight =
        pairSize.height
        + pairBaseline;

    const int secondLineHeight =
        std::max(
            visibleSize.height
                + visibleBaseline,
            thermalSize.height
                + thermalBaseline
        );

    const int headerHeight =
        kTextPadding
        + firstLineHeight
        + kLineGap
        + secondLineHeight
        + kTextPadding;

    const int fullWidth =
        sourceWidth * 2;

    cv::Mat header =
        cv::Mat::zeros(
            headerHeight,
            fullWidth,
            CV_8UC3
        );

    const int firstLineY =
        kTextPadding
        + pairSize.height;

    const int secondLineY =
        kTextPadding
        + firstLineHeight
        + kLineGap
        + std::max(
            visibleSize.height,
            thermalSize.height
        );

    const int pairX =
        std::max(
            0,
            (
                fullWidth
                - pairSize.width
            )
            / 2
        );

    const int visibleX =
        std::max(
            0,
            (
                sourceWidth
                - visibleSize.width
            )
            / 2
        );

    const int thermalX =
        std::max(
            sourceWidth,
            sourceWidth
            + (
                sourceWidth
                - thermalSize.width
            )
            / 2
        );

    const cv::Scalar textColor(
        255,
        255,
        255
    );

    cv::putText(
        header,
        pairText,
        cv::Point(
            pairX,
            firstLineY
        ),
        kFontFace,
        kFontScale,
        textColor,
        kFontThickness,
        cv::LINE_AA
    );

    cv::putText(
        header,
        visibleText,
        cv::Point(
            visibleX,
            secondLineY
        ),
        kFontFace,
        kFontScale,
        textColor,
        kFontThickness,
        cv::LINE_AA
    );

    cv::putText(
        header,
        thermalText,
        cv::Point(
            thermalX,
            secondLineY
        ),
        kFontFace,
        kFontScale,
        textColor,
        kFontThickness,
        cv::LINE_AA
    );

    return header;
}


std::filesystem::path
makeTemporaryPngPath(
    const std::filesystem::path& outputPath
)
{
    std::random_device randomDevice;

    for (
        int attempt = 0;
        attempt < 32;
        ++attempt
    ) {
        const std::string nonce =
            std::to_string(
                randomDevice()
            )
            + "-"
            + std::to_string(
                randomDevice()
            );

        const auto candidate =
            outputPath.parent_path()
            / (
                "."
                + outputPath.stem().string()
                + "."
                + nonce
                + ".png"
            );

        if (
            !std::filesystem::exists(
                candidate
            )
        ) {
            return candidate;
        }
    }

    throw PreviewRenderError(
        "Could not allocate a temporary "
        "preview PNG path."
    );
}


void synchronizeFile(
    const std::filesystem::path& path
)
{
#ifdef _WIN32
    std::FILE* file =
        _wfopen(
            path.c_str(),
            L"rb+"
        );
#else
    std::FILE* file =
        std::fopen(
            path.c_str(),
            "rb+"
        );
#endif

    if (file == nullptr) {
        throw PreviewRenderError(
            "Could not open temporary preview "
            "PNG for synchronization."
        );
    }

    try {
        if (
            std::fflush(file) != 0
        ) {
            throw PreviewRenderError(
                "Could not flush temporary preview PNG."
            );
        }

#ifdef _WIN32
        const int descriptor =
            _fileno(file);

        if (
            descriptor < 0
            || _commit(descriptor) != 0
        ) {
            throw PreviewRenderError(
                "Could not synchronize temporary preview PNG."
            );
        }
#else
        const int descriptor =
            fileno(file);

        if (
            descriptor < 0
            || fsync(descriptor) != 0
        ) {
            throw PreviewRenderError(
                "Could not synchronize temporary preview PNG."
            );
        }
#endif

        if (
            std::fclose(file) != 0
        ) {
            file = nullptr;

            throw PreviewRenderError(
                "Could not close temporary preview PNG."
            );
        }

        file = nullptr;
    }
    catch (...) {
        if (file != nullptr) {
            std::fclose(file);
        }

        throw;
    }
}


void replaceTemporaryFile(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& outputPath,
    const bool overwrite
)
{
#ifdef _WIN32
    DWORD flags =
        MOVEFILE_WRITE_THROUGH;

    if (overwrite) {
        flags |=
            MOVEFILE_REPLACE_EXISTING;
    }

    if (
        !MoveFileExW(
            temporaryPath.c_str(),
            outputPath.c_str(),
            flags
        )
    ) {
        throw std::system_error(
            static_cast<int>(
                GetLastError()
            ),
            std::system_category(),
            "Could not replace preview PNG"
        );
    }
#else
    if (
        !overwrite
        && std::filesystem::exists(
            outputPath
        )
    ) {
        throw PreviewRenderError(
            "Preview output already exists and "
            "overwrite is false: "
            + outputPath.string()
        );
    }

    std::filesystem::rename(
        temporaryPath,
        outputPath
    );
#endif
}


void removeTemporaryFile(
    const std::filesystem::path& path
) noexcept
{
    std::error_code error;

    std::filesystem::remove(
        path,
        error
    );
}


void writeAtomicPng(
    const cv::Mat& image,
    const std::filesystem::path& outputPath,
    const bool overwrite
)
{
    try {
        if (
            std::filesystem::exists(
                outputPath
            )
            && std::filesystem::is_directory(
                outputPath
            )
        ) {
            throw PreviewRenderError(
                "Preview output path is a directory: "
                + outputPath.string()
            );
        }

        if (
            std::filesystem::exists(
                outputPath
            )
            && !overwrite
        ) {
            throw PreviewRenderError(
                "Preview output already exists and "
                "overwrite is false: "
                + outputPath.string()
            );
        }

        std::filesystem::create_directories(
            outputPath.parent_path()
        );

        const auto temporaryPath =
            makeTemporaryPngPath(
                outputPath
            );

        try {
            bool writeSucceeded = false;

            try {
                writeSucceeded =
                    cv::imwrite(
                        temporaryPath.string(),
                        image
                    );
            }
            catch (const cv::Exception&) {
                throw PreviewRenderError(
                    "OpenCV could not encode the "
                    "preview PNG: "
                    + outputPath.string()
                );
            }

            if (!writeSucceeded) {
                throw PreviewRenderError(
                    "OpenCV could not encode the "
                    "preview PNG: "
                    + outputPath.string()
                );
            }

            synchronizeFile(
                temporaryPath
            );

            /*
             * Preserve no-overwrite semantics even if the
             * destination appeared while encoding.
             */
            if (
                !overwrite
                && std::filesystem::exists(
                    outputPath
                )
            ) {
                throw PreviewRenderError(
                    "Preview output already exists and "
                    "overwrite is false: "
                    + outputPath.string()
                );
            }

            replaceTemporaryFile(
                temporaryPath,
                outputPath,
                overwrite
            );
        }
        catch (...) {
            removeTemporaryFile(
                temporaryPath
            );

            throw;
        }
    }
    catch (const PreviewRenderError&) {
        throw;
    }
    catch (
        const std::filesystem::filesystem_error&
    ) {
        throw PreviewRenderError(
            "Could not write preview PNG: "
            + outputPath.string()
        );
    }
    catch (const std::system_error&) {
        throw PreviewRenderError(
            "Could not write preview PNG: "
            + outputPath.string()
        );
    }
}


} // namespace


PreviewRenderResult
OpenCvPreviewRenderer::render(
    const PreviewRenderRequest& request
) const
{
    const auto& visibleDimensions =
        request.result()
            .visibleDimensions();

    const auto& thermalDimensions =
        request.result()
            .thermalDimensions();

    if (
        !visibleDimensions.has_value()
        || !thermalDimensions.has_value()
    ) {
        throw PreviewRenderError(
            "VALID preview request lacks "
            "source dimensions."
        );
    }

    const cv::Mat visibleImage =
        loadDisplayImage(
            request.visibleSourcePath(),
            "Visible"
        );

    const cv::Mat thermalImage =
        loadDisplayImage(
            request.thermalSourcePath(),
            "Thermal"
        );

    validateLoadedDimensions(
        visibleImage,
        visibleDimensions.value(),
        "Visible",
        request.visibleSourcePath()
    );

    validateLoadedDimensions(
        thermalImage,
        thermalDimensions.value(),
        "Thermal",
        request.thermalSourcePath()
    );

    if (
        visibleImage.rows
            != thermalImage.rows
        || visibleImage.cols
            != thermalImage.cols
    ) {
        throw PreviewRenderError(
            "Visible and thermal preview sources "
            "must have matching spatial dimensions."
        );
    }

    const int sourceWidth =
        visibleImage.cols;

    const int sourceHeight =
        visibleImage.rows;

    const cv::Mat header =
        buildHeader(
            sourceWidth,
            request.result()
                .pair()
                .pairId()
        );

    cv::Mat body;

    cv::hconcat(
        visibleImage,
        thermalImage,
        body
    );

    cv::Mat preview;

    cv::vconcat(
        header,
        body,
        preview
    );

    writeAtomicPng(
        preview,
        request.outputPath(),
        request.overwrite()
    );

    return PreviewRenderResult(
        request.result()
            .pair()
            .pairId(),
        request.outputPath(),
        ImageDimensions(
            sourceWidth,
            sourceHeight
        ),
        ImageDimensions(
            preview.cols,
            preview.rows
        ),
        header.rows
    );
}

} // namespace qart::core::visualization