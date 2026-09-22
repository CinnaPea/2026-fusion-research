//
// Created by hakgu on 8/11/2026.
//

#include "core/imaging/opencv_image_decoder.h"

#include "core/domain/imaging/image_decode_result.h"
#include "core/domain/imaging/image_decode_status.h"
#include "core/domain/imaging/image_dimensions.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <string>
#include <system_error>

namespace qart::core::imaging {

domain::imaging::ImageDecodeResult
OpenCvImageDecoder::decode(
    const std::filesystem::path& imagePath
)
{
    using domain::imaging::ImageDecodeResult;
    using domain::imaging::ImageDecodeStatus;
    using domain::imaging::ImageDimensions;

    const std::string pathText =
        imagePath.string();

    std::error_code fileStatusError;

    const bool isFile =
        std::filesystem::is_regular_file(
            imagePath,
            fileStatusError
        );

    if (!isFile || fileStatusError) {
        return ImageDecodeResult(
            ImageDecodeStatus::DecodeFailed,
            std::nullopt,
            "Image file does not exist in "
                + pathText
        );
    }

    cv::Mat decoded;

    try {
        decoded = cv::imread(
            pathText,
            cv::IMREAD_UNCHANGED
        );
    }
    catch (const cv::Exception& error) {
        return ImageDecodeResult(
            ImageDecodeStatus::DecodeFailed,
            std::nullopt,
            "OpenCV raised an error while decoding "
                + pathText
                + " : "
                + error.what()
        );
    }

    if (decoded.empty()) {
        return ImageDecodeResult(
            ImageDecodeStatus::DecodeFailed,
            std::nullopt,
            "OpenCV could not decode image file: "
                + pathText
        );
    }

    if (decoded.dims < 2) {
        return ImageDecodeResult(
            ImageDecodeStatus::DecodeFailed,
            std::nullopt,
            "Decoded image does not contain valid "
            "2D spatial data: "
                + pathText
        );
    }

    const int width = decoded.cols;
    const int height = decoded.rows;

    if (width <= 0 || height <= 0) {
        return ImageDecodeResult(
            ImageDecodeStatus::DecodeFailed,
            std::nullopt,
            "Decoded image contains non-positive "
            "dimensions: "
                + pathText
        );
    }

    return ImageDecodeResult(
        ImageDecodeStatus::Decoded,
        ImageDimensions(
            width,
            height
        )
    );
}

} // namespace qart::core::imaging