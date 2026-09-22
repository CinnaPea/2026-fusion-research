//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/imaging/image_dimensions.h"

#include <stdexcept>

namespace {

    using qart::core::domain::imaging::ImageDimensions;

    bool rejectsDimensions(
        const int width,
        const int height
    )
    {
        try {
            const ImageDimensions dimensions(
                width,
                height
            );

            static_cast<void>(dimensions);
        }
        catch (const std::invalid_argument&) {
            return true;
        }

        return false;
    }

} // namespace

int main()
{
    const ImageDimensions llvip(
        1280,
        1024
    );

    if (llvip.width() != 1280) {
        return 1;
    }

    if (llvip.height() != 1024) {
        return 2;
    }

    const ImageDimensions msrs(
        640,
        480
    );

    if (msrs.width() != 640) {
        return 3;
    }

    if (msrs.height() != 480) {
        return 4;
    }

    const ImageDimensions minimum(
        1,
        1
    );

    if (
        minimum.width() != 1
        || minimum.height() != 1
    ) {
        return 5;
    }

    if (
        ImageDimensions(1280, 1024)
        != ImageDimensions(1280, 1024)
    ) {
        return 6;
    }

    if (
        ImageDimensions(1280, 1024)
        == ImageDimensions(640, 480)
    ) {
        return 7;
    }

    if (!rejectsDimensions(0, 1024)) {
        return 8;
    }

    if (!rejectsDimensions(-1, 1024)) {
        return 9;
    }

    if (!rejectsDimensions(1280, 0)) {
        return 10;
    }

    if (!rejectsDimensions(1280, -1)) {
        return 11;
    }

    return 0;
}