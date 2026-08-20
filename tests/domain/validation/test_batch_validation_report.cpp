//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/batch_validation_report.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/batch_validation_summary.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/domain/validation/validation_status_count.h"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::imaging::ImageDimensions;
using qart::core::domain::validation::BatchValidationReport;
using qart::core::domain::validation::BatchValidationSummary;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;
using qart::core::domain::validation::ValidationStatusCount;

constexpr std::array<PairValidationStatus, 12>
allStatuses = {
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
    const std::string& sourceStem
)
{
    return DatasetPair(
        "llvip_train_" + sourceStem,
        "llvip",
        DatasetPartitionId("train"),
        sourceStem,
        "raw/llvip/visible/train/"
            + sourceStem
            + ".jpg",
        "raw/llvip/infrared/train/"
            + sourceStem
            + ".jpg"
    );
}

PairValidationResult validResult(
    const DatasetPair& pair
)
{
    const ImageDimensions dimensions(
        1280,
        1024
    );

    return PairValidationResult(
        pair,
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}

PairValidationResult missingVisibleResult(
    const DatasetPair& pair
)
{
    return PairValidationResult(
        pair,
        PairValidationStatus::MissingVisible,
        false,
        true,
        std::nullopt,
        ImageDimensions(1280, 1024),
        std::string(
            "Visible source file was not found."
        )
    );
}

std::vector<ValidationStatusCount> statusCounts(
    const int validCount,
    const int missingVisibleCount
)
{
    std::vector<ValidationStatusCount> counts;

    for (const PairValidationStatus status : allStatuses) {
        int count = 0;

        if (status == PairValidationStatus::Valid) {
            count = validCount;
        }
        else if (
            status
            == PairValidationStatus::MissingVisible
        ) {
            count = missingVisibleCount;
        }

        counts.emplace_back(
            status,
            count
        );
    }

    return counts;
}

BatchValidationSummary makeSummary(
    const int totalPairs,
    const int validPairs,
    const int invalidPairs,
    const int validCount,
    const int missingVisibleCount
)
{
    return BatchValidationSummary(
        totalPairs,
        validPairs,
        invalidPairs,
        statusCounts(
            validCount,
            missingVisibleCount
        )
    );
}

bool rejectsResultCountMismatch()
{
    try {
        const DatasetPair pair =
            makePair("010001");

        const BatchValidationReport report(
            {
                validResult(pair)
            },
            makeSummary(
                2,
                2,
                0,
                2,
                0
            )
        );

        static_cast<void>(report);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsDuplicatePairIds()
{
    try {
        const DatasetPair pair =
            makePair("010001");

        const BatchValidationReport report(
            {
                validResult(pair),
                validResult(pair)
            },
            makeSummary(
                2,
                2,
                0,
                2,
                0
            )
        );

        static_cast<void>(report);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

bool rejectsStatusFrequencyMismatch()
{
    try {
        const DatasetPair pair =
            makePair("010001");

        const BatchValidationReport report(
            {
                validResult(pair)
            },
            makeSummary(
                1,
                0,
                1,
                0,
                1
            )
        );

        static_cast<void>(report);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const BatchValidationReport empty(
        {},
        makeSummary(
            0,
            0,
            0,
            0,
            0
        )
    );

    if (!empty.results().empty()) {
        return 1;
    }

    if (empty.summary().totalPairs() != 0) {
        return 2;
    }

    const DatasetPair firstPair =
        makePair("010003");

    const DatasetPair secondPair =
        makePair("010001");

    const BatchValidationReport ordered(
        {
            validResult(firstPair),
            missingVisibleResult(secondPair)
        },
        makeSummary(
            2,
            1,
            1,
            1,
            1
        )
    );

    if (ordered.results().size() != 2) {
        return 3;
    }

    if (
        ordered.results().at(0).pair().pairId()
        != "llvip_train_010003"
    ) {
        return 4;
    }

    if (
        ordered.results().at(1).pair().pairId()
        != "llvip_train_010001"
    ) {
        return 5;
    }

    if (
        ordered.summary().countFor(
            PairValidationStatus::Valid
        )
        != 1
    ) {
        return 6;
    }

    if (
        ordered.summary().countFor(
            PairValidationStatus::MissingVisible
        )
        != 1
    ) {
        return 7;
    }

    const BatchValidationReport equivalent(
        {
            validResult(firstPair),
            missingVisibleResult(secondPair)
        },
        makeSummary(
            2,
            1,
            1,
            1,
            1
        )
    );

    if (ordered != equivalent) {
        return 8;
    }

    if (!rejectsResultCountMismatch()) {
        return 9;
    }

    if (!rejectsDuplicatePairIds()) {
        return 10;
    }

    if (!rejectsStatusFrequencyMismatch()) {
        return 11;
    }

    return 0;
}