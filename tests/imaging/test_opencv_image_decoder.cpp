//
// Created by hakgu on 8/11/2026.
//

#include "core/imaging/opencv_image_decoder.h"

#include "core/domain/imaging/image_decode_status.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/imaging/image_decoder.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

namespace {

using qart::core::domain::imaging::ImageDecodeStatus;
using qart::core::domain::imaging::ImageDimensions;
using qart::core::imaging::ImageDecoder;
using qart::core::imaging::OpenCvImageDecoder;

class TemporaryTestDirectory final
{
public:
    TemporaryTestDirectory()
        : path_(
            std::filesystem::temp_directory_path()
            / "qart_opencv_image_decoder_contract"
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

    ~TemporaryTestDirectory()
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

bool containsText(
    const std::optional<std::string>& message,
    const std::string& expected
)
{
    return message.has_value()
        && message->find(expected)
            != std::string::npos;
}

} // namespace

int main()
{
    static_assert(
        std::is_base_of_v<
            ImageDecoder,
            OpenCvImageDecoder
        >,
        "OpenCvImageDecoder must implement ImageDecoder."
    );

    TemporaryTestDirectory temporaryDirectory;

    const std::filesystem::path root =
        temporaryDirectory.path();

    OpenCvImageDecoder decoder;

    const std::filesystem::path grayscalePath =
        root / "grayscale.png";

    const cv::Mat grayscale =
        cv::Mat::zeros(
            7,
            11,
            CV_8UC1
        );

    if (
        !cv::imwrite(
            grayscalePath.string(),
            grayscale
        )
    ) {
        return 1;
    }

    const auto grayscaleResult =
        decoder.decode(grayscalePath);

    if (!grayscaleResult.succeeded()) {
        return 2;
    }

    if (
        grayscaleResult.status()
        != ImageDecodeStatus::Decoded
    ) {
        return 3;
    }

    if (!grayscaleResult.dimensions().has_value()) {
        return 4;
    }

    if (
        grayscaleResult.dimensions().value()
        != ImageDimensions(11, 7)
    ) {
        return 5;
    }

    if (grayscaleResult.message().has_value()) {
        return 6;
    }

    const std::filesystem::path colorPath =
        root / "color.png";

    const cv::Mat color =
        cv::Mat::zeros(
            13,
            17,
            CV_8UC3
        );

    if (
        !cv::imwrite(
            colorPath.string(),
            color
        )
    ) {
        return 7;
    }

    const auto colorResult =
        decoder.decode(colorPath);

    if (!colorResult.succeeded()) {
        return 8;
    }

    if (
        !colorResult.dimensions().has_value()
        || colorResult.dimensions().value()
            != ImageDimensions(17, 13)
    ) {
        return 9;
    }

    const std::filesystem::path missingPath =
        root / "missing-image.jpg";

    const auto missingResult =
        decoder.decode(missingPath);

    if (missingResult.succeeded()) {
        return 10;
    }

    if (
        missingResult.status()
        != ImageDecodeStatus::DecodeFailed
    ) {
        return 11;
    }

    if (missingResult.dimensions().has_value()) {
        return 12;
    }

    if (
        !containsText(
            missingResult.message(),
            "Image file does not exist"
        )
    ) {
        return 13;
    }

    const std::filesystem::path corruptPath =
        root / "corrupt-image.jpg";

    {
        std::ofstream corruptFile(
            corruptPath,
            std::ios::binary
        );

        if (!corruptFile) {
            return 14;
        }

        corruptFile
            << "This is not encoded image data.";
    }

    const auto corruptResult =
        decoder.decode(corruptPath);

    if (corruptResult.succeeded()) {
        return 15;
    }

    if (
        corruptResult.status()
        != ImageDecodeStatus::DecodeFailed
    ) {
        return 16;
    }

    if (corruptResult.dimensions().has_value()) {
        return 17;
    }

    if (
        !containsText(
            corruptResult.message(),
            "OpenCV could not decode image file"
        )
    ) {
        return 18;
    }

    return 0;
}