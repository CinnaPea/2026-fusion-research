//
// Created by hakgu on 8/11/2026.
//

#include "image_decode_status.h"
#include <stdexcept>

namespace qart::core::domain::imaging {
    std::string_view toString(const ImageDecodeStatus sts) noexcept {
        switch (sts) {
            case ImageDecodeStatus::Decoded:
                return "DECODED";
            case ImageDecodeStatus::DecodeFailed:
                return "DECODE_FAILED";
        }
        return "";
    }

    ImageDecodeStatus imageDecodeStatusFromString(const std::string& val) {
        if (val == "DECODED") return ImageDecodeStatus::Decoded;
        if (val == "DECODE_FAILED") return ImageDecodeStatus::DecodeFailed;
        throw std::invalid_argument("Unknown image decode status: " + val);
    }
}