//
// Created by hakgu on 8/11/2026.
//

#pragma once

#include "../domain/imaging/image_decode_result.h"
#include <filesystem>

#ifndef VISUAL_THERMAL_CONCEPT_IMAGE_DECODER_H
#define VISUAL_THERMAL_CONCEPT_IMAGE_DECODER_H

namespace qart::core::imaging {
    class ImageDecoder {
    public:
        virtual ~ImageDecoder() = default;

        [[nodiscard]]
        virtual domain::imaging::ImageDecodeResult decode(const std::filesystem::path& imgPath) = 0;
    };
}

#endif //VISUAL_THERMAL_CONCEPT_IMAGE_DECODER_H