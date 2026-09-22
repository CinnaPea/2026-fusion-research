//
// Created by hakgu on 8/16/2026.
//

#include "core/domain/validation/batch_validation_progress.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <limits>
#include <stdexcept>

namespace {

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::imaging::ImageDimensions;
using qart::core::domain::validation::BatchValidationProgress;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;

PairValidationResult makeValidResult()
{
    const DatasetPair pair(
        "llvip_train_010001",
        "llvip",
        DatasetPartitionId("train"),
        "010001",
        "raw/llvip/visible/train/010001.jpg",
        "raw/llvip/infrared/train/010001.jpg"
    );

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

bool rejectsProgress(
    const int completedPairs,
    const int totalPairs,
    const double pairElapsedSeconds,
    const double batchElapsedSeconds
)
{
    try {
        const BatchValidationProgress progress(
            completedPairs,
            totalPairs,
            makeValidResult(),
            pairElapsedSeconds,
            batchElapsedSeconds
        );

        static_cast<void>(progress);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const PairValidationResult currentResult =
        makeValidResult();

    const BatchValidationProgress progress(
        2,
        5,
        currentResult,
        1.25,
        4.0
    );

    if (progress.completedPairs() != 2) {
        return 1;
    }

    if (progress.totalPairs() != 5) {
        return 2;
    }

    if (progress.currentResult() != currentResult) {
        return 3;
    }

    if (progress.pairElapsedSeconds() != 1.25) {
        return 4;
    }

    if (progress.batchElapsedSeconds() != 4.0) {
        return 5;
    }

    if (progress.remainingPairs() != 3) {
        return 6;
    }

    if (progress.averageSecondsPerPair() != 2.0) {
        return 7;
    }

    if (
        progress.estimatedRemainingSeconds()
        != 6.0
    ) {
        return 8;
    }

    const BatchValidationProgress finished(
        5,
        5,
        currentResult,
        0.75,
        10.0
    );

    if (finished.remainingPairs() != 0) {
        return 9;
    }

    if (
        finished.estimatedRemainingSeconds()
        != 0.0
    ) {
        return 10;
    }

    const BatchValidationProgress equivalent(
        2,
        5,
        currentResult,
        1.25,
        4.0
    );

    if (progress != equivalent) {
        return 11;
    }

    if (
        !rejectsProgress(
            1,
            0,
            1.0,
            1.0
        )
    ) {
        return 12;
    }

    if (
        !rejectsProgress(
            0,
            5,
            1.0,
            1.0
        )
    ) {
        return 13;
    }

    if (
        !rejectsProgress(
            6,
            5,
            1.0,
            1.0
        )
    ) {
        return 14;
    }

    if (
        !rejectsProgress(
            1,
            5,
            -0.1,
            1.0
        )
    ) {
        return 15;
    }

    if (
        !rejectsProgress(
            1,
            5,
            0.1,
            -1.0
        )
    ) {
        return 16;
    }

    if (
        !rejectsProgress(
            1,
            5,
            std::numeric_limits<double>::infinity(),
            1.0
        )
    ) {
        return 17;
    }

    if (
        !rejectsProgress(
            1,
            5,
            1.0,
            std::numeric_limits<double>::infinity()
        )
    ) {
        return 18;
    }

    if (
        !rejectsProgress(
            1,
            5,
            std::numeric_limits<double>::quiet_NaN(),
            1.0
        )
    ) {
        return 19;
    }

    if (
        !rejectsProgress(
            1,
            5,
            1.0,
            std::numeric_limits<double>::quiet_NaN()
        )
    ) {
        return 20;
    }

    return 0;
}