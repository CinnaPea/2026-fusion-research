//
// Created by hakgu on 8/14/2026.
//

#include "core/domain/validation/pair_validation_policy.h"

#include "core/domain/imaging/image_dimensions.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::core::domain::imaging::ImageDimensions;
using qart::core::domain::validation::PairValidationPolicy;

bool rejectsVisibleExtensions(
    const std::vector<std::string>& extensions
)
{
    try {
        const PairValidationPolicy policy(
            extensions,
            {".jpg"}
        );

        static_cast<void>(policy);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsThermalExtensions(
    const std::vector<std::string>& extensions
)
{
    try {
        const PairValidationPolicy policy(
            {".jpg"},
            extensions
        );

        static_cast<void>(policy);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const PairValidationPolicy normalized(
        {
            ".PNG",
            " .jpg ",
            ".JPG",
            ".bmp"
        },
        {
            " .PNG ",
            ".jpg",
            ".png"
        }
    );

    const std::vector<std::string>
        expectedVisible = {
            ".bmp",
            ".jpg",
            ".png"
        };

    const std::vector<std::string>
        expectedThermal = {
            ".jpg",
            ".png"
        };

    if (
        normalized.supportedVisibleExtensions()
        != expectedVisible
    ) {
        return 1;
    }

    if (
        normalized.supportedThermalExtensions()
        != expectedThermal
    ) {
        return 2;
    }

    if (
        normalized.expectedVisibleDimensions()
            .has_value()
    ) {
        return 3;
    }

    if (
        normalized.expectedThermalDimensions()
            .has_value()
    ) {
        return 4;
    }

    const PairValidationPolicy withExpectedDimensions(
        {".jpg"},
        {".png"},
        ImageDimensions(1280, 1024),
        ImageDimensions(1280, 1024)
    );

    if (
        !withExpectedDimensions
            .expectedVisibleDimensions()
            .has_value()
    ) {
        return 5;
    }

    if (
        !withExpectedDimensions
            .expectedThermalDimensions()
            .has_value()
    ) {
        return 6;
    }

    if (
        withExpectedDimensions
            .expectedVisibleDimensions()
            .value()
        != ImageDimensions(1280, 1024)
    ) {
        return 7;
    }

    if (
        withExpectedDimensions
            .expectedThermalDimensions()
            .value()
        != ImageDimensions(1280, 1024)
    ) {
        return 8;
    }

    const PairValidationPolicy equivalent(
        {".JPG"},
        {".PNG"},
        ImageDimensions(1280, 1024),
        ImageDimensions(1280, 1024)
    );

    if (
        withExpectedDimensions
        != equivalent
    ) {
        return 9;
    }

    if (
        !rejectsVisibleExtensions({})
    ) {
        return 10;
    }

    if (
        !rejectsThermalExtensions({})
    ) {
        return 11;
    }

    if (
        !rejectsVisibleExtensions({""})
    ) {
        return 12;
    }

    if (
        !rejectsVisibleExtensions({"   "})
    ) {
        return 13;
    }

    if (
        !rejectsVisibleExtensions({"."})
    ) {
        return 14;
    }

    if (
        !rejectsVisibleExtensions({"jpg"})
    ) {
        return 15;
    }

    if (
        !rejectsVisibleExtensions({".jp/g"})
    ) {
        return 16;
    }

    if (
        !rejectsVisibleExtensions({".jp\\g"})
    ) {
        return 17;
    }

    if (
        !rejectsThermalExtensions({"png"})
    ) {
        return 18;
    }

    return 0;
}