//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/imaging/image_decode_result.h"

#include <stdexcept>
#include <utility>

namespace qart::core::domain::imaging {

    ImageDecodeResult::ImageDecodeResult(
        const ImageDecodeStatus status,
        std::optional<ImageDimensions> dimensions,
        std::optional<std::string> message
    )
        : status_(status),
          dimensions_(std::move(dimensions)),
          message_(std::move(message))
    {
        validate();
    }

    ImageDecodeStatus
    ImageDecodeResult::status() const noexcept
    {
        return status_;
    }

    const std::optional<ImageDimensions>&
    ImageDecodeResult::dimensions() const noexcept
    {
        return dimensions_;
    }

    const std::optional<std::string>&
    ImageDecodeResult::message() const noexcept
    {
        return message_;
    }

    bool ImageDecodeResult::succeeded() const noexcept
    {
        return status_ == ImageDecodeStatus::Decoded;
    }

    void ImageDecodeResult::validate() const
    {
        if (
            status_ == ImageDecodeStatus::Decoded
            && !dimensions_.has_value()
        ) {
            throw std::invalid_argument(
                "A DECODED result must include image dimensions."
            );
        }

        if (
            status_ == ImageDecodeStatus::DecodeFailed
            && dimensions_.has_value()
        ) {
            throw std::invalid_argument(
                "A DECODE_FAILED result must not include dimensions."
            );
        }
    }

    bool operator==(
        const ImageDecodeResult& left,
        const ImageDecodeResult& right
    ) noexcept
    {
        return left.status_ == right.status_
            && left.dimensions_ == right.dimensions_
            && left.message_ == right.message_;
    }

    bool operator!=(
        const ImageDecodeResult& left,
        const ImageDecodeResult& right
    ) noexcept
    {
        return !(left == right);
    }

} // namespace qart::core::domain::imaging