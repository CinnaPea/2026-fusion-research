//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/imaging/image_decode_status.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

    using qart::core::domain::imaging::ImageDecodeStatus;
    using qart::core::domain::imaging::imageDecodeStatusFromString;
    using qart::core::domain::imaging::toString;

    struct StatusCase
    {
        ImageDecodeStatus status;
        std::string text;
    };

    bool rejectsInvalidStatus(
        const std::string& value
    )
    {
        try {
            const ImageDecodeStatus status =
                imageDecodeStatusFromString(value);

            static_cast<void>(status);
        }
        catch (const std::invalid_argument&) {
            return true;
        }

        return false;
    }

} // namespace

int main()
{
    const std::vector<StatusCase> validCases = {
        {
            ImageDecodeStatus::Decoded,
            "DECODED"
        },
        {
            ImageDecodeStatus::DecodeFailed,
            "DECODE_FAILED"
        }
    };

    for (const StatusCase& testCase : validCases) {
        if (toString(testCase.status) != testCase.text) {
            return 1;
        }

        if (
            imageDecodeStatusFromString(testCase.text)
            != testCase.status
        ) {
            return 2;
        }
    }

    const std::vector<std::string> invalidValues = {
        "",
        "decoded",
        "Decoded",
        " DECODED",
        "DECODED ",
        "DECODE-FAILED",
        "FAILED",
        "UNKNOWN"
    };

    for (const std::string& invalidValue : invalidValues) {
        if (!rejectsInvalidStatus(invalidValue)) {
            return 3;
        }
    }

    return 0;
}