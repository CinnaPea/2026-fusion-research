//
// Created by hakgu on 8/11/2026.
//

#include "core/imaging/image_decoder.h"

#include "core/domain/imaging/image_decode_result.h"
#include "core/domain/imaging/image_decode_status.h"
#include "core/domain/imaging/image_dimensions.h"

#include <filesystem>
#include <optional>
#include <type_traits>
#include <utility>

namespace {

using qart::core::domain::imaging::ImageDecodeResult;
using qart::core::domain::imaging::ImageDecodeStatus;
using qart::core::domain::imaging::ImageDimensions;
using qart::core::imaging::ImageDecoder;

class FakeImageDecoder final : public ImageDecoder
{
public:
    explicit FakeImageDecoder(
        ImageDecodeResult result
    )
        : result_(std::move(result))
    {
    }

    ImageDecodeResult decode(
        const std::filesystem::path& imagePath
    ) override
    {
        lastImagePath_ = imagePath;
        return result_;
    }

    [[nodiscard]]
    const std::optional<std::filesystem::path>&
    lastImagePath() const noexcept
    {
        return lastImagePath_;
    }

private:
    ImageDecodeResult result_;
    std::optional<std::filesystem::path> lastImagePath_;
};

} // namespace

int main()
{
    static_assert(
        std::is_abstract_v<ImageDecoder>,
        "ImageDecoder must remain an abstract interface."
    );

    const ImageDecodeResult expected(
        ImageDecodeStatus::Decoded,
        ImageDimensions(1280, 1024)
    );

    FakeImageDecoder decoder(expected);

    const std::filesystem::path imagePath(
        "F:/QART_Datasets/raw/llvip/"
        "visible/train/010001.jpg"
    );

    const ImageDecodeResult result =
        decoder.decode(imagePath);

    if (result != expected) {
        return 1;
    }

    if (!decoder.lastImagePath().has_value()) {
        return 2;
    }

    if (
        decoder.lastImagePath().value()
        != imagePath
    ) {
        return 3;
    }

    const ImageDecodeResult failedResult(
        ImageDecodeStatus::DecodeFailed,
        std::nullopt,
        "Expected test failure."
    );

    FakeImageDecoder failedDecoder(
        failedResult
    );

    const std::filesystem::path thermalPath(
        "F:/QART_Datasets/raw/llvip/"
        "infrared/test/190001.jpg"
    );

    const ImageDecodeResult observedFailure =
        failedDecoder.decode(thermalPath);

    if (observedFailure != failedResult) {
        return 4;
    }

    if (
        !failedDecoder.lastImagePath().has_value()
        || failedDecoder.lastImagePath().value()
            != thermalPath
    ) {
        return 5;
    }

    return 0;
}