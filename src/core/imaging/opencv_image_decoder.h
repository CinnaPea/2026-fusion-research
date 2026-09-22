//
// Created by hakgu on 8/11/2026.
//
#pragma once
#include "image_decoder.h"

#ifndef VISUAL_THERMAL_CONCEPT_OPENCV_IMAGE_DECODER_H
#define VISUAL_THERMAL_CONCEPT_OPENCV_IMAGE_DECODER_H

namespace qart::core::imaging {
    class OpenCvImageDecoder final : public ImageDecoder {
    public:
        [[nodiscard]]
        domain::imaging::ImageDecodeResult decode(const std::filesystem::path& imgPath) override;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_OPENCV_IMAGE_DECODER_H