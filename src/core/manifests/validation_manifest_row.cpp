//
// Created by hakgu on 8/18/2026.
//

#include "core/manifests/validation_manifest_row.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_status.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace qart::core::manifests {

namespace {

bool isBlank(
    const std::string& value
) noexcept
{
    return value.find_first_not_of(
        " \t\n\r\f\v"
    ) == std::string::npos;
}

std::string optionalIntegerText(
    const std::optional<int>& value
)
{
    return value.has_value()
        ? std::to_string(value.value())
        : std::string{};
}

std::optional<
    domain::imaging::ImageDimensions
> buildDimensions(
    const std::optional<int>& width,
    const std::optional<int>& height,
    const std::string& modalityName
)
{
    if (
        !width.has_value()
        && !height.has_value()
    ) {
        return std::nullopt;
    }

    if (
        !width.has_value()
        || !height.has_value()
    ) {
        throw std::invalid_argument(
            modalityName
            + " width and height must either both "
              "exist or both be absent."
        );
    }

    return domain::imaging::ImageDimensions(
        width.value(),
        height.value()
    );
}

} // namespace

ValidationManifestRow::ValidationManifestRow(
    std::string pairId,
    std::string datasetId,
    std::string partitionId,
    std::string sourceStem,
    std::string visibleRelativePath,
    std::string thermalRelativePath,
    std::string status,
    const bool visibleDecoded,
    const bool thermalDecoded,
    std::optional<int> visibleWidth,
    std::optional<int> visibleHeight,
    std::optional<int> thermalWidth,
    std::optional<int> thermalHeight,
    std::optional<std::string> message
)
    : pairId_(
          std::move(pairId)
      ),
      datasetId_(
          std::move(datasetId)
      ),
      partitionId_(
          std::move(partitionId)
      ),
      sourceStem_(
          std::move(sourceStem)
      ),
      visibleRelativePath_(
          std::move(visibleRelativePath)
      ),
      thermalRelativePath_(
          std::move(thermalRelativePath)
      ),
      status_(
          std::move(status)
      ),
      visibleDecoded_(
          visibleDecoded
      ),
      thermalDecoded_(
          thermalDecoded
      ),
      visibleWidth_(
          visibleWidth
      ),
      visibleHeight_(
          visibleHeight
      ),
      thermalWidth_(
          thermalWidth
      ),
      thermalHeight_(
          thermalHeight
      ),
      message_(
          std::move(message)
      )
{
    validate();
}

void ValidationManifestRow::validateDimensionPair(
    const std::optional<int>& width,
    const std::optional<int>& height,
    const std::string_view modalityName
)
{
    if (
        width.has_value()
        != height.has_value()
    ) {
        throw std::invalid_argument(
            std::string(modalityName)
            + " width and height must either both "
              "exist or both be absent."
        );
    }

    if (
        width.has_value()
        && width.value() <= 0
    ) {
        throw std::invalid_argument(
            std::string(modalityName)
            + " width must be positive."
        );
    }

    if (
        height.has_value()
        && height.value() <= 0
    ) {
        throw std::invalid_argument(
            std::string(modalityName)
            + " height must be positive."
        );
    }
}

void ValidationManifestRow::validate() const
{
    const std::array<
        std::pair<std::string_view, const std::string*>,
        7
    > requiredTextFields = {{
        {
            "pair_id",
            &pairId_
        },
        {
            "dataset_id",
            &datasetId_
        },
        {
            "partition_id",
            &partitionId_
        },
        {
            "source_stem",
            &sourceStem_
        },
        {
            "visible_relative_path",
            &visibleRelativePath_
        },
        {
            "thermal_relative_path",
            &thermalRelativePath_
        },
        {
            "status",
            &status_
        }
    }};

    for (
        const auto& field
        : requiredTextFields
    ) {
        if (isBlank(*field.second)) {
            throw std::invalid_argument(
                "Manifest field '"
                + std::string(field.first)
                + "' must not be empty."
            );
        }
    }

    static_cast<void>(
        domain::datasets::DatasetPartitionId(
            partitionId_
        )
    );

    validateDimensionPair(
        visibleWidth_,
        visibleHeight_,
        "Visible"
    );

    validateDimensionPair(
        thermalWidth_,
        thermalHeight_,
        "Thermal"
    );
}

ValidationManifestRow
ValidationManifestRow::fromResult(
    const domain::validation::PairValidationResult& result
)
{
    const auto& pair =
        result.pair();

    std::optional<int> visibleWidth;
    std::optional<int> visibleHeight;

    if (
        result.visibleDimensions().has_value()
    ) {
        visibleWidth =
            result.visibleDimensions()
                .value()
                .width();

        visibleHeight =
            result.visibleDimensions()
                .value()
                .height();
    }

    std::optional<int> thermalWidth;
    std::optional<int> thermalHeight;

    if (
        result.thermalDimensions().has_value()
    ) {
        thermalWidth =
            result.thermalDimensions()
                .value()
                .width();

        thermalHeight =
            result.thermalDimensions()
                .value()
                .height();
    }

    return ValidationManifestRow(
        pair.pairId(),
        pair.datasetId(),
        pair.partitionId().value(),
        pair.sourceStem(),
        pair.visibleRelativePath(),
        pair.thermalRelativePath(),
        std::string(
            domain::validation::toString(
                result.status()
            )
        ),
        result.visibleDecoded(),
        result.thermalDecoded(),
        visibleWidth,
        visibleHeight,
        thermalWidth,
        thermalHeight,
        result.message()
    );
}

domain::validation::PairValidationResult
ValidationManifestRow::toResult() const
{
    const domain::datasets::DatasetPartitionId
        partitionId(
            partitionId_
        );

    domain::validation::PairValidationStatus
        parsedStatus;

    try {
        parsedStatus =
            domain::validation::
                pairValidationStatusFromString(
                    status_
                );
    }
    catch (const std::invalid_argument&) {
        throw std::invalid_argument(
            "Unknown validation status '"
            + status_
            + "'."
        );
    }

    const domain::datasets::DatasetPair pair(
        pairId_,
        datasetId_,
        partitionId,
        sourceStem_,
        visibleRelativePath_,
        thermalRelativePath_
    );

    const auto visibleDimensions =
        buildDimensions(
            visibleWidth_,
            visibleHeight_,
            "Visible"
        );

    const auto thermalDimensions =
        buildDimensions(
            thermalWidth_,
            thermalHeight_,
            "Thermal"
        );

    return domain::validation::PairValidationResult(
        pair,
        parsedStatus,
        visibleDecoded_,
        thermalDecoded_,
        visibleDimensions,
        thermalDimensions,
        message_
    );
}

std::array<
    std::string,
    kValidationManifestColumnCount
> ValidationManifestRow::toCsvValues() const
{
    return {
        std::string(
            kValidationManifestSchemaVersion
        ),
        pairId_,
        datasetId_,
        partitionId_,
        sourceStem_,
        visibleRelativePath_,
        thermalRelativePath_,
        status_,
        visibleDecoded_
            ? "true"
            : "false",
        thermalDecoded_
            ? "true"
            : "false",
        optionalIntegerText(
            visibleWidth_
        ),
        optionalIntegerText(
            visibleHeight_
        ),
        optionalIntegerText(
            thermalWidth_
        ),
        optionalIntegerText(
            thermalHeight_
        ),
        message_.value_or(
            std::string{}
        )
    };
}

const std::string&
ValidationManifestRow::pairId() const noexcept
{
    return pairId_;
}

const std::string&
ValidationManifestRow::datasetId() const noexcept
{
    return datasetId_;
}

const std::string&
ValidationManifestRow::partitionId() const noexcept
{
    return partitionId_;
}

const std::string&
ValidationManifestRow::sourceStem() const noexcept
{
    return sourceStem_;
}

const std::string&
ValidationManifestRow::visibleRelativePath() const noexcept
{
    return visibleRelativePath_;
}

const std::string&
ValidationManifestRow::thermalRelativePath() const noexcept
{
    return thermalRelativePath_;
}

const std::string&
ValidationManifestRow::status() const noexcept
{
    return status_;
}

bool ValidationManifestRow::visibleDecoded() const noexcept
{
    return visibleDecoded_;
}

bool ValidationManifestRow::thermalDecoded() const noexcept
{
    return thermalDecoded_;
}

const std::optional<int>&
ValidationManifestRow::visibleWidth() const noexcept
{
    return visibleWidth_;
}

const std::optional<int>&
ValidationManifestRow::visibleHeight() const noexcept
{
    return visibleHeight_;
}

const std::optional<int>&
ValidationManifestRow::thermalWidth() const noexcept
{
    return thermalWidth_;
}

const std::optional<int>&
ValidationManifestRow::thermalHeight() const noexcept
{
    return thermalHeight_;
}

const std::optional<std::string>&
ValidationManifestRow::message() const noexcept
{
    return message_;
}

bool operator==(
    const ValidationManifestRow& left,
    const ValidationManifestRow& right
) noexcept
{
    return left.pairId_
            == right.pairId_
        && left.datasetId_
            == right.datasetId_
        && left.partitionId_
            == right.partitionId_
        && left.sourceStem_
            == right.sourceStem_
        && left.visibleRelativePath_
            == right.visibleRelativePath_
        && left.thermalRelativePath_
            == right.thermalRelativePath_
        && left.status_
            == right.status_
        && left.visibleDecoded_
            == right.visibleDecoded_
        && left.thermalDecoded_
            == right.thermalDecoded_
        && left.visibleWidth_
            == right.visibleWidth_
        && left.visibleHeight_
            == right.visibleHeight_
        && left.thermalWidth_
            == right.thermalWidth_
        && left.thermalHeight_
            == right.thermalHeight_
        && left.message_
            == right.message_;
}

bool operator!=(
    const ValidationManifestRow& left,
    const ValidationManifestRow& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::manifests