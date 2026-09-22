//
// Created by hakgu on 8/11/2026.
//

#pragma once

#ifndef VISUAL_THERMAL_CONCEPT_IMAGE_DIMENSIONS_H
#define VISUAL_THERMAL_CONCEPT_IMAGE_DIMENSIONS_H

namespace qart::core::domain::imaging {
    class ImageDimensions final {
    public:
        ImageDimensions(int w, int h);

        [[nodiscard]]
        int width() const noexcept;

        [[nodiscard]]
        int height() const noexcept;

        friend bool operator==(const ImageDimensions &lhs, const ImageDimensions &rhs) noexcept;
        friend bool operator!=(const ImageDimensions &lhs, const ImageDimensions &rhs) noexcept;
    private:
        int w_;
        int h_;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_IMAGE_DIMENSIONS_H