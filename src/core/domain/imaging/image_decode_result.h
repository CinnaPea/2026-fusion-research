//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include "image_decode_status.h"
#include "image_dimensions.h"
#include <optional>
#include <string>

#ifndef VISUAL_THERMAL_CONCEPT_IMAGE_DECODE_RESULT_H
#define VISUAL_THERMAL_CONCEPT_IMAGE_DECODE_RESULT_H

namespace qart::core::domain::imaging {

    class ImageDecodeResult final
    {
    public:
        ImageDecodeResult(
            ImageDecodeStatus status,
            std::optional<ImageDimensions> dimensions = std::nullopt,
            std::optional<std::string> message = std::nullopt
        );

        [[nodiscard]]
        ImageDecodeStatus status() const noexcept;

        [[nodiscard]]
        const std::optional<ImageDimensions>&
        dimensions() const noexcept;

        [[nodiscard]]
        const std::optional<std::string>&
        message() const noexcept;

        [[nodiscard]]
        bool succeeded() const noexcept;

        friend bool operator==(
            const ImageDecodeResult& left,
            const ImageDecodeResult& right
        ) noexcept;

        friend bool operator!=(
            const ImageDecodeResult& left,
            const ImageDecodeResult& right
        ) noexcept;

    private:
        void validate() const;

        ImageDecodeStatus status_;
        std::optional<ImageDimensions> dimensions_;
        std::optional<std::string> message_;
    };

}

#endif //VISUAL_THERMAL_CONCEPT_IMAGE_DECODE_RESULT_H