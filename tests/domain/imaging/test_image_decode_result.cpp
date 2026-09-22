//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/imaging/image_decode_result.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace {

using qart::core::domain::imaging::ImageDecodeResult;
using qart::core::domain::imaging::ImageDecodeStatus;
using qart::core::domain::imaging::ImageDimensions;

bool rejectsDecodedWithoutDimensions()
{
    try {
        const ImageDecodeResult result(
            ImageDecodeStatus::Decoded,
            std::nullopt
        );

        static_cast<void>(result);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsFailedWithDimensions()
{
    try {
        const ImageDecodeResult result(
            ImageDecodeStatus::DecodeFailed,
            ImageDimensions(1280, 1024)
        );

        static_cast<void>(result);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const ImageDecodeResult decoded(
        ImageDecodeStatus::Decoded,
        ImageDimensions(1280, 1024)
    );

    if (!decoded.succeeded()) {
        return 1;
    }

    if (decoded.status() != ImageDecodeStatus::Decoded) {
        return 2;
    }

    if (!decoded.dimensions().has_value()) {
        return 3;
    }

    if (
        decoded.dimensions().value()
        != ImageDimensions(1280, 1024)
    ) {
        return 4;
    }

    if (decoded.message().has_value()) {
        return 5;
    }

    const ImageDecodeResult failed(
        ImageDecodeStatus::DecodeFailed,
        std::nullopt,
        std::string(
            "Decoder could not read the image."
        )
    );

    if (failed.succeeded()) {
        return 6;
    }

    if (
        failed.status()
        != ImageDecodeStatus::DecodeFailed
    ) {
        return 7;
    }

    if (failed.dimensions().has_value()) {
        return 8;
    }

    if (!failed.message().has_value()) {
        return 9;
    }

    if (
        failed.message().value()
        != "Decoder could not read the image."
    ) {
        return 10;
    }

    if (!rejectsDecodedWithoutDimensions()) {
        return 11;
    }

    if (!rejectsFailedWithDimensions()) {
        return 12;
    }

    const ImageDecodeResult equivalent(
        ImageDecodeStatus::Decoded,
        ImageDimensions(1280, 1024)
    );

    if (decoded != equivalent) {
        return 13;
    }

    const ImageDecodeResult failedWithoutMessage(
        ImageDecodeStatus::DecodeFailed
    );

    if (failedWithoutMessage.succeeded()) {
        return 14;
    }

    if (failedWithoutMessage.message().has_value()) {
        return 15;
    }

    return 0;
}