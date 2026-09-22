//
// Created by hakgu on 8/15/2026.
//

#include "core/validation/pair_validator.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/imaging/image_decode_result.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_status.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace qart::core::validation {

namespace {

std::string toLower(
    std::string value
)
{
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(character)
            )
        );
    }

    return value;
}

std::string dimensionsText(
    const domain::imaging::ImageDimensions& dimensions
)
{
    return std::to_string(dimensions.width())
        + "x"
        + std::to_string(dimensions.height());
}

} // namespace

PairValidator::PairValidator(
    imaging::ImageDecoder& decoder,
    domain::validation::PairValidationPolicy policy
)
    : decoder_(decoder),
      policy_(std::move(policy))
{
}

std::filesystem::path PairValidator::resolvePhysicalPath(
    const std::filesystem::path& datasetRoot,
    const std::string& relativePath
)
{
    std::filesystem::path resolved =
        datasetRoot;

    std::size_t componentStart = 0;

    while (componentStart <= relativePath.size()) {
        const std::size_t separator =
            relativePath.find(
                '/',
                componentStart
            );

        const std::size_t componentLength =
            separator == std::string::npos
                ? relativePath.size() - componentStart
                : separator - componentStart;

        resolved /= relativePath.substr(
            componentStart,
            componentLength
        );

        if (separator == std::string::npos) {
            break;
        }

        componentStart = separator + 1;
    }

    return resolved;
}

bool PairValidator::extensionIsSupported(
    const std::filesystem::path& imagePath,
    const std::vector<std::string>& supportedExtensions
)
{
    const std::string extension =
        toLower(
            imagePath.extension().string()
        );

    return std::find(
        supportedExtensions.begin(),
        supportedExtensions.end(),
        extension
    ) != supportedExtensions.end();
}

std::optional<domain::imaging::ImageDecodeResult>
PairValidator::decodeWhenEligible(
    const std::filesystem::path& imagePath,
    const bool fileExists,
    const bool extensionSupported
)
{
    if (!fileExists || !extensionSupported) {
        return std::nullopt;
    }

    return decoder_.decode(imagePath);
}

domain::validation::PairValidationResult
PairValidator::validatePair(
    const std::filesystem::path& datasetRoot,
    const domain::datasets::DatasetPair& pair
)
{
    using domain::datasets::DatasetLayoutError;
    using domain::datasets::DatasetPairingError;
    using domain::imaging::ImageDecodeResult;
    using domain::imaging::ImageDimensions;
    using domain::validation::PairValidationResult;
    using domain::validation::PairValidationStatus;

    if (!std::filesystem::exists(datasetRoot)) {
        throw DatasetLayoutError(
            "Shared dataset root does not exist: "
            + datasetRoot.string()
        );
    }

    if (!std::filesystem::is_directory(datasetRoot)) {
        throw DatasetLayoutError(
            "Shared dataset root is not a directory: "
            + datasetRoot.string()
        );
    }

    const std::filesystem::path visiblePath =
        resolvePhysicalPath(
            datasetRoot,
            pair.visibleRelativePath()
        );

    const std::filesystem::path thermalPath =
        resolvePhysicalPath(
            datasetRoot,
            pair.thermalRelativePath()
        );

    const bool visibleExists =
        std::filesystem::is_regular_file(
            visiblePath
        );

    const bool thermalExists =
        std::filesystem::is_regular_file(
            thermalPath
        );

    if (!visibleExists && !thermalExists) {
        throw DatasetPairingError(
            "Candidate pair '"
            + pair.pairId()
            + "' references no existing visible "
              "or thermal source file."
        );
    }

    const bool visibleExtensionSupported =
        visibleExists
        && extensionIsSupported(
            visiblePath,
            policy_.supportedVisibleExtensions()
        );

    const bool thermalExtensionSupported =
        thermalExists
        && extensionIsSupported(
            thermalPath,
            policy_.supportedThermalExtensions()
        );

    const std::optional<ImageDecodeResult>
        visibleDecodeResult =
            decodeWhenEligible(
                visiblePath,
                visibleExists,
                visibleExtensionSupported
            );

    const std::optional<ImageDecodeResult>
        thermalDecodeResult =
            decodeWhenEligible(
                thermalPath,
                thermalExists,
                thermalExtensionSupported
            );

    const bool visibleDecoded =
        visibleDecodeResult.has_value()
        && visibleDecodeResult->succeeded();

    const bool thermalDecoded =
        thermalDecodeResult.has_value()
        && thermalDecodeResult->succeeded();

    const std::optional<ImageDimensions>
        visibleDimensions =
            visibleDecoded
                ? visibleDecodeResult->dimensions()
                : std::nullopt;

    const std::optional<ImageDimensions>
        thermalDimensions =
            thermalDecoded
                ? thermalDecodeResult->dimensions()
                : std::nullopt;

    if (!visibleExists) {
        return PairValidationResult(
            pair,
            PairValidationStatus::MissingVisible,
            false,
            thermalDecoded,
            std::nullopt,
            thermalDimensions,
            "Visible source file does not exist: "
                + visiblePath.string()
        );
    }

    if (!thermalExists) {
        return PairValidationResult(
            pair,
            PairValidationStatus::MissingThermal,
            visibleDecoded,
            false,
            visibleDimensions,
            std::nullopt,
            "Thermal source file does not exist: "
                + thermalPath.string()
        );
    }

    if (!visibleExtensionSupported) {
        return PairValidationResult(
            pair,
            PairValidationStatus::UnsupportedVisibleFormat,
            false,
            thermalDecoded,
            std::nullopt,
            thermalDimensions,
            "Visible source uses an unsupported extension: "
                + visiblePath.extension().string()
        );
    }

    if (!thermalExtensionSupported) {
        return PairValidationResult(
            pair,
            PairValidationStatus::UnsupportedThermalFormat,
            visibleDecoded,
            false,
            visibleDimensions,
            std::nullopt,
            "Thermal source uses an unsupported extension: "
                + thermalPath.extension().string()
        );
    }

    if (!visibleDecoded) {
        std::optional<std::string> message;

        if (
            visibleDecodeResult.has_value()
            && visibleDecodeResult->message().has_value()
        ) {
            message =
                visibleDecodeResult->message();
        }
        else {
            message =
                "Visible source could not be decoded: "
                + visiblePath.string();
        }

        return PairValidationResult(
            pair,
            PairValidationStatus::VisibleDecodeFailed,
            false,
            thermalDecoded,
            std::nullopt,
            thermalDimensions,
            std::move(message)
        );
    }

    if (!thermalDecoded) {
        std::optional<std::string> message;

        if (
            thermalDecodeResult.has_value()
            && thermalDecodeResult->message().has_value()
        ) {
            message =
                thermalDecodeResult->message();
        }
        else {
            message =
                "Thermal source could not be decoded: "
                + thermalPath.string();
        }

        return PairValidationResult(
            pair,
            PairValidationStatus::ThermalDecodeFailed,
            true,
            false,
            visibleDimensions,
            std::nullopt,
            std::move(message)
        );
    }

    if (
        !visibleDimensions.has_value()
        || !thermalDimensions.has_value()
    ) {
        throw std::runtime_error(
            "Decoder contract violation: a successful "
            "decode must include image dimensions."
        );
    }

    if (
        visibleDimensions.value()
        != thermalDimensions.value()
    ) {
        return PairValidationResult(
            pair,
            PairValidationStatus::DimensionMismatch,
            true,
            true,
            visibleDimensions,
            thermalDimensions,
            "Visible and thermal dimensions do not match: "
                "visible="
                + dimensionsText(
                    visibleDimensions.value()
                )
                + ", thermal="
                + dimensionsText(
                    thermalDimensions.value()
                )
                + "."
        );
    }

    const bool visibleDimensionsUnexpected =
        policy_.expectedVisibleDimensions().has_value()
        && visibleDimensions.value()
            != policy_.expectedVisibleDimensions().value();

    const bool thermalDimensionsUnexpected =
        policy_.expectedThermalDimensions().has_value()
        && thermalDimensions.value()
            != policy_.expectedThermalDimensions().value();

    if (visibleDimensionsUnexpected) {
        std::string message =
            "Visible dimensions differ from the configured "
            "expectation: actual="
            + dimensionsText(
                visibleDimensions.value()
            )
            + ", expected="
            + dimensionsText(
                policy_.expectedVisibleDimensions().value()
            )
            + ".";

        if (thermalDimensionsUnexpected) {
            message +=
                " Thermal dimensions also differ from "
                "the configured expectation.";
        }

        return PairValidationResult(
            pair,
            PairValidationStatus::UnexpectedVisibleDimensions,
            true,
            true,
            visibleDimensions,
            thermalDimensions,
            std::move(message)
        );
    }

    if (thermalDimensionsUnexpected) {
        if (
            !policy_
                 .expectedThermalDimensions()
                 .has_value()
        ) {
            throw std::runtime_error(
                "Thermal expectation state is inconsistent."
            );
        }

        return PairValidationResult(
            pair,
            PairValidationStatus::UnexpectedThermalDimensions,
            true,
            true,
            visibleDimensions,
            thermalDimensions,
            "Thermal dimensions differ from the configured "
            "expectation: actual="
                + dimensionsText(
                    thermalDimensions.value()
                )
                + ", expected="
                + dimensionsText(
                    policy_
                        .expectedThermalDimensions()
                        .value()
                )
                + "."
        );
    }

    return PairValidationResult(
        pair,
        PairValidationStatus::Valid,
        true,
        true,
        visibleDimensions,
        thermalDimensions
    );
}

} // namespace qart::core::validation