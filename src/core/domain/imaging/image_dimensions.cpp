//
// Created by hakgu on 8/11/2026.
//

#include "image_dimensions.h"
#include<stdexcept>

namespace qart::core::domain::imaging {
    ImageDimensions::ImageDimensions(const int w, const int h) : w_(w), h_(h) {
        if (w_ <= 0) throw std::invalid_argument("w must be positive");
        if (h_ <= 0) throw std::invalid_argument("h must be positive");
    }

    int ImageDimensions::height() const noexcept {
        return h_;
    }
    int ImageDimensions::width() const noexcept {
        return w_;
    }
    bool operator==(const ImageDimensions &lhs, const ImageDimensions &rhs) noexcept {
        return lhs.w_ == rhs.w_ && lhs.h_ == rhs.h_;
    }
    bool operator!=(const ImageDimensions &lhs, const ImageDimensions &rhs) noexcept {
        return !(lhs == rhs);
    }
}