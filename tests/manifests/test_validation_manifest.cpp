//
// Created by hakgu on 8/18/2026.
//

#include "core/manifests/validation_manifest.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/batch_validation_report.h"
#include "core/domain/validation/batch_validation_summary.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/domain/validation/validation_status_count.h"
#include "core/manifests/validation_manifest_row.h"

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::core::domain::datasets::
    DatasetPair;
using qart::core::domain::datasets::
    DatasetPartitionId;

using qart::core::domain::imaging::
    ImageDimensions;

using qart::core::domain::validation::
    BatchValidationReport;
using qart::core::domain::validation::
    BatchValidationSummary;
using qart::core::domain::validation::
    PairValidationResult;
using qart::core::domain::validation::
    PairValidationStatus;
using qart::core::domain::validation::
    ValidationStatusCount;

using qart::core::manifests::
    ValidationManifest;
using qart::core::manifests::
    ValidationManifestRow;
using qart::core::manifests::
    kValidationManifestSchemaVersion;

constexpr std::array<
    PairValidationStatus,
    12
> allStatuses = {
    PairValidationStatus::Valid,

    PairValidationStatus::MissingVisible,
    PairValidationStatus::MissingThermal,

    PairValidationStatus::UnsupportedVisibleFormat,
    PairValidationStatus::UnsupportedThermalFormat,

    PairValidationStatus::VisibleDecodeFailed,
    PairValidationStatus::ThermalDecodeFailed,

    PairValidationStatus::DimensionMismatch,
    PairValidationStatus::UnexpectedVisibleDimensions,
    PairValidationStatus::UnexpectedThermalDimensions,

    PairValidationStatus::DuplicateVisibleStem,
    PairValidationStatus::DuplicateThermalStem
};

DatasetPair makePair(
    const std::string& stem
)
{
    return DatasetPair(
        "llvip_train_" + stem,
        "llvip",
        DatasetPartitionId("train"),
        stem,
        "raw/llvip/visible/train/"
            + stem
            + ".jpg",
        "raw/llvip/infrared/train/"
            + stem
            + ".jpg"
    );
}

PairValidationResult validResult(
    const std::string& stem
)
{
    const ImageDimensions dimensions(
        1280,
        1024
    );

    return PairValidationResult(
        makePair(stem),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}

PairValidationResult missingVisibleResult(
    const std::string& stem
)
{
    return PairValidationResult(
        makePair(stem),
        PairValidationStatus::MissingVisible,
        false,
        true,
        std::nullopt,
        ImageDimensions(
            1280,
            1024
        ),
        std::string(
            "Visible source file was not found."
        )
    );
}

BatchValidationSummary makeSummary()
{
    std::vector<ValidationStatusCount>
        counts;

    counts.reserve(
        allStatuses.size()
    );

    for (
        const PairValidationStatus status
        : allStatuses
    ) {
        int count = 0;

        if (
            status
            == PairValidationStatus::Valid
        ) {
            count = 1;
        }
        else if (
            status
            == PairValidationStatus::MissingVisible
        ) {
            count = 1;
        }

        counts.emplace_back(
            status,
            count
        );
    }

    return BatchValidationSummary(
        2,
        1,
        1,
        std::move(counts)
    );
}

BatchValidationReport makeReport()
{
    return BatchValidationReport(
        {
            validResult("010003"),
            missingVisibleResult("010001")
        },
        makeSummary()
    );
}

} // namespace

int main()
{
    // ------------------------------------------------------------
    // Empty Schema 2.0 manifest is valid.
    // ------------------------------------------------------------

    const ValidationManifest emptyManifest(
        "2.0",
        {}
    );

    if (
        emptyManifest.schemaVersion()
        != kValidationManifestSchemaVersion
    ) {
        return 1;
    }

    if (!emptyManifest.rows().empty()) {
        return 2;
    }

    if (!emptyManifest.toResults().empty()) {
        return 3;
    }

    // ------------------------------------------------------------
    // Report -> manifest preserves report order.
    // ------------------------------------------------------------

    const BatchValidationReport report =
        makeReport();

    const ValidationManifest manifest =
        ValidationManifest::fromReport(
            report
        );

    if (
        manifest.schemaVersion()
        != "2.0"
    ) {
        return 4;
    }

    if (manifest.rows().size() != 2) {
        return 5;
    }

    if (
        manifest.rows().at(0).pairId()
        != "llvip_train_010003"
    ) {
        return 6;
    }

    if (
        manifest.rows().at(1).pairId()
        != "llvip_train_010001"
    ) {
        return 7;
    }

    // ------------------------------------------------------------
    // Manifest -> results preserves typed values and order.
    // ------------------------------------------------------------

    const auto reconstructedResults =
        manifest.toResults();

    if (reconstructedResults.size() != 2) {
        return 8;
    }

    if (
        reconstructedResults.at(0)
        != report.results().at(0)
    ) {
        return 9;
    }

    if (
        reconstructedResults.at(1)
        != report.results().at(1)
    ) {
        return 10;
    }

    // ------------------------------------------------------------
    // Value semantics.
    // ------------------------------------------------------------

    const ValidationManifest equivalent =
        ValidationManifest::fromReport(
            report
        );

    if (manifest != equivalent) {
        return 11;
    }

    // ------------------------------------------------------------
    // Duplicate pair IDs are rejected.
    // ------------------------------------------------------------

    const ValidationManifestRow duplicateRow =
        ValidationManifestRow::fromResult(
            validResult("010005")
        );

    try {
        const ValidationManifest duplicateManifest(
            "2.0",
            {
                duplicateRow,
                duplicateRow
            }
        );

        static_cast<void>(
            duplicateManifest
        );

        return 12;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Empty/blank schema version rejected.
    // ------------------------------------------------------------

    try {
        const ValidationManifest invalidSchema(
            "   ",
            {}
        );

        static_cast<void>(
            invalidSchema
        );

        return 13;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Legacy Schema 1.0 deliberately rejected in C++.
    // ------------------------------------------------------------

    try {
        const ValidationManifest legacyManifest(
            "1.0",
            {}
        );

        static_cast<void>(
            legacyManifest
        );

        return 14;
    }
    catch (const std::invalid_argument&) {
    }

    // ------------------------------------------------------------
    // Unknown future schema rejected.
    // ------------------------------------------------------------

    try {
        const ValidationManifest futureManifest(
            "3.0",
            {}
        );

        static_cast<void>(
            futureManifest
        );

        return 15;
    }
    catch (const std::invalid_argument&) {
    }

    return 0;
}