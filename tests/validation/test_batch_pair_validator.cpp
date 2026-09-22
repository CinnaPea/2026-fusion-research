//
// Created by hakgu on 8/17/2026.
//

#include "core/validation/batch_pair_validator.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_decode_result.h"
#include "core/domain/imaging/image_decode_status.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/batch_validation_progress.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/imaging/image_decoder.h"
#include "core/validation/pair_validator.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using qart::core::domain::datasets::DatasetLayoutError;
using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPairingError;
using qart::core::domain::datasets::DatasetPartitionId;

using qart::core::domain::imaging::ImageDecodeResult;
using qart::core::domain::imaging::ImageDecodeStatus;
using qart::core::domain::imaging::ImageDimensions;

using qart::core::domain::validation::BatchValidationProgress;
using qart::core::domain::validation::PairValidationPolicy;
using qart::core::domain::validation::PairValidationStatus;

using qart::core::imaging::ImageDecoder;

using qart::core::validation::BatchPairValidator;
using qart::core::validation::PairValidator;

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

class FakeImageDecoder final : public ImageDecoder
{
public:
    void setResult(
        const std::filesystem::path& path,
        ImageDecodeResult result
    )
    {
        results_.insert_or_assign(
            path,
            std::move(result)
        );
    }

    void setFailurePath(
        std::filesystem::path path
    )
    {
        failurePath_ =
            std::move(path);
    }

    ImageDecodeResult decode(
        const std::filesystem::path& imagePath
    ) override
    {
        calls_.push_back(
            imagePath
        );

        if (
            failurePath_.has_value()
            && imagePath
                == failurePath_.value()
        ) {
            throw DatasetLayoutError(
                "Injected layout failure."
            );
        }

        const auto result =
            results_.find(imagePath);

        if (result == results_.end()) {
            return ImageDecodeResult(
                ImageDecodeStatus::DecodeFailed,
                std::nullopt,
                "No fake decode result configured."
            );
        }

        return result->second;
    }

    [[nodiscard]]
    const std::vector<std::filesystem::path>&
    calls() const noexcept
    {
        return calls_;
    }

private:
    std::map<
        std::filesystem::path,
        ImageDecodeResult
    > results_;

    std::optional<std::filesystem::path>
        failurePath_;

    std::vector<std::filesystem::path>
        calls_;
};

class SequenceClock final
{
public:
    explicit SequenceClock(
        std::vector<double> values
    )
        : values_(std::move(values))
    {
    }

    double operator()()
    {
        if (nextIndex_ >= values_.size()) {
            throw std::runtime_error(
                "SequenceClock exhausted."
            );
        }

        const double value =
            values_.at(nextIndex_);

        ++nextIndex_;

        return value;
    }

    [[nodiscard]]
    std::size_t callCount() const noexcept
    {
        return nextIndex_;
    }

private:
    std::vector<double> values_;
    std::size_t nextIndex_ = 0;
};

class RecordingProgressObserver final
{
public:
    void operator()(
        const BatchValidationProgress& progress
    )
    {
        observations_.push_back(
            progress
        );
    }

    [[nodiscard]]
    const std::vector<BatchValidationProgress>&
    observations() const noexcept
    {
        return observations_;
    }

private:
    std::vector<BatchValidationProgress>
        observations_;
};

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
            std::filesystem::temp_directory_path()
            / "qart_batch_pair_validator_contract"
        )
    {
        std::error_code error;

        std::filesystem::remove_all(
            path_,
            error
        );

        std::filesystem::create_directories(
            path_
        );
    }

    ~TemporaryDirectory()
    {
        std::error_code error;

        std::filesystem::remove_all(
            path_,
            error
        );
    }

    [[nodiscard]]
    const std::filesystem::path&
    path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
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
        "visible/" + sourceStem + ".jpg",
        "thermal/" + sourceStem + ".jpg"
    );
}

void createFile(
    const std::filesystem::path& path
)
{
    std::filesystem::create_directories(
        path.parent_path()
    );

    std::ofstream stream(
        path,
        std::ios::binary
    );

    stream << "fixture";
}

ImageDecodeResult decoded()
{
    return ImageDecodeResult(
        ImageDecodeStatus::Decoded,
        ImageDimensions(1280, 1024)
    );
}

ImageDecodeResult decodeFailed()
{
    return ImageDecodeResult(
        ImageDecodeStatus::DecodeFailed,
        std::nullopt,
        "Thermal source could not be decoded."
    );
}

PairValidationPolicy policy()
{
    return PairValidationPolicy(
        {".jpg"},
        {".jpg"},
        ImageDimensions(1280, 1024),
        ImageDimensions(1280, 1024)
    );
}

} // namespace

int main()
{
    TemporaryDirectory temporaryDirectory;

    const std::filesystem::path root =
        temporaryDirectory.path();

    std::filesystem::create_directories(
        root / "visible"
    );

    std::filesystem::create_directories(
        root / "thermal"
    );

    // ------------------------------------------------------------
    // Empty batch:
    // no validation, no clock, complete zero summary.
    // ------------------------------------------------------------

    {
        FakeImageDecoder decoder;
        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {100.0}
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        const auto report =
            batchValidator.validatePairs(
                root,
                {}
            );

        if (!report.results().empty()) {
            return 1;
        }

        if (
            report.summary().totalPairs() != 0
            || report.summary().validPairs() != 0
            || report.summary().invalidPairs() != 0
        ) {
            return 2;
        }

        if (clock.callCount() != 0) {
            return 3;
        }

        if (!decoder.calls().empty()) {
            return 4;
        }

        if (
            report.summary().statusCounts().size()
            != allStatuses.size()
        ) {
            return 5;
        }

        for (
            std::size_t index = 0;
            index < allStatuses.size();
            ++index
        ) {
            if (
                report.summary()
                    .statusCounts()
                    .at(index)
                    .status()
                != allStatuses.at(index)
            ) {
                return 6;
            }

            if (
                report.summary()
                    .statusCounts()
                    .at(index)
                    .count()
                != 0
            ) {
                return 7;
            }
        }
    }

    // ------------------------------------------------------------
    // Mixed batch:
    // preserve input order and build complete summary.
    // ------------------------------------------------------------

    {
        const DatasetPair validPair =
            makePair("010003");

        const DatasetPair missingVisiblePair =
            makePair("010001");

        const DatasetPair decodeFailedPair =
            makePair("010002");

        const auto validVisible =
            root / "visible/010003.jpg";

        const auto validThermal =
            root / "thermal/010003.jpg";

        const auto missingVisibleThermal =
            root / "thermal/010001.jpg";

        const auto failedVisible =
            root / "visible/010002.jpg";

        const auto failedThermal =
            root / "thermal/010002.jpg";

        createFile(validVisible);
        createFile(validThermal);
        createFile(missingVisibleThermal);
        createFile(failedVisible);
        createFile(failedThermal);

        FakeImageDecoder decoder;

        decoder.setResult(
            validVisible,
            decoded()
        );

        decoder.setResult(
            validThermal,
            decoded()
        );

        decoder.setResult(
            missingVisibleThermal,
            decoded()
        );

        decoder.setResult(
            failedVisible,
            decoded()
        );

        decoder.setResult(
            failedThermal,
            decodeFailed()
        );

        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {
                10.0,
                11.0,
                11.5,
                12.0,
                12.5,
                13.0,
                14.0
            }
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        RecordingProgressObserver observer;

        const std::vector<DatasetPair> pairs = {
            validPair,
            missingVisiblePair,
            decodeFailedPair
        };

        const auto report =
            batchValidator.validatePairs(
                root,
                pairs,
                std::ref(observer)
            );

        if (report.results().size() != 3) {
            return 8;
        }

        if (
            report.results().at(0).pair().pairId()
            != validPair.pairId()
        ) {
            return 9;
        }

        if (
            report.results().at(1).pair().pairId()
            != missingVisiblePair.pairId()
        ) {
            return 10;
        }

        if (
            report.results().at(2).pair().pairId()
            != decodeFailedPair.pairId()
        ) {
            return 11;
        }

        if (report.summary().totalPairs() != 3) {
            return 12;
        }

        if (report.summary().validPairs() != 1) {
            return 13;
        }

        if (report.summary().invalidPairs() != 2) {
            return 14;
        }

        if (
            report.summary().countFor(
                PairValidationStatus::Valid
            )
            != 1
        ) {
            return 15;
        }

        if (
            report.summary().countFor(
                PairValidationStatus::MissingVisible
            )
            != 1
        ) {
            return 16;
        }

        if (
            report.summary().countFor(
                PairValidationStatus::ThermalDecodeFailed
            )
            != 1
        ) {
            return 17;
        }

        if (
            observer.observations().size()
            != 3
        ) {
            return 18;
        }

        const auto& firstProgress =
            observer.observations().at(0);

        if (
            firstProgress.completedPairs() != 1
            || firstProgress.totalPairs() != 3
            || firstProgress.currentResult()
                .pair()
                .pairId()
                != validPair.pairId()
        ) {
            return 19;
        }

        if (
            firstProgress.pairElapsedSeconds()
            != 0.5
        ) {
            return 20;
        }

        if (
            firstProgress.batchElapsedSeconds()
            != 1.5
        ) {
            return 21;
        }

        const auto& finalProgress =
            observer.observations().at(2);

        if (
            finalProgress.completedPairs() != 3
            || finalProgress.remainingPairs() != 0
        ) {
            return 22;
        }

        if (
            finalProgress.pairElapsedSeconds()
            != 1.0
        ) {
            return 23;
        }

        if (
            finalProgress.batchElapsedSeconds()
            != 4.0
        ) {
            return 24;
        }
    }

    // ------------------------------------------------------------
    // Duplicate IDs are rejected before clock/validation.
    // ------------------------------------------------------------

    {
        const DatasetPair pair =
            makePair("020001");

        FakeImageDecoder decoder;

        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {1.0}
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        try {
            static_cast<void>(
                batchValidator.validatePairs(
                    root,
                    {
                        pair,
                        pair
                    }
                )
            );

            return 25;
        }
        catch (const DatasetPairingError&) {
        }

        if (clock.callCount() != 0) {
            return 26;
        }

        if (!decoder.calls().empty()) {
            return 27;
        }
    }

    // ------------------------------------------------------------
    // Infrastructure failure propagates and stops later pairs.
    // ------------------------------------------------------------

    {
        const DatasetPair firstPair =
            makePair("030001");

        const DatasetPair failingPair =
            makePair("030002");

        const DatasetPair thirdPair =
            makePair("030003");

        const auto firstVisible =
            root / "visible/030001.jpg";

        const auto firstThermal =
            root / "thermal/030001.jpg";

        const auto failingVisible =
            root / "visible/030002.jpg";

        const auto failingThermal =
            root / "thermal/030002.jpg";

        const auto thirdVisible =
            root / "visible/030003.jpg";

        const auto thirdThermal =
            root / "thermal/030003.jpg";

        createFile(firstVisible);
        createFile(firstThermal);
        createFile(failingVisible);
        createFile(failingThermal);
        createFile(thirdVisible);
        createFile(thirdThermal);

        FakeImageDecoder decoder;

        decoder.setResult(
            firstVisible,
            decoded()
        );

        decoder.setResult(
            firstThermal,
            decoded()
        );

        decoder.setFailurePath(
            failingVisible
        );

        decoder.setResult(
            thirdVisible,
            decoded()
        );

        decoder.setResult(
            thirdThermal,
            decoded()
        );

        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {
                0.0,
                1.0,
                2.0,
                3.0
            }
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        try {
            static_cast<void>(
                batchValidator.validatePairs(
                    root,
                    {
                        firstPair,
                        failingPair,
                        thirdPair
                    }
                )
            );

            return 28;
        }
        catch (const DatasetLayoutError& error) {
            if (
                std::string(error.what())
                != "Injected layout failure."
            ) {
                return 29;
            }
        }

        for (
            const std::filesystem::path& call
            : decoder.calls()
        ) {
            if (
                call == thirdVisible
                || call == thirdThermal
            ) {
                return 30;
            }
        }
    }

    // ------------------------------------------------------------
    // Non-finite batch start: fail before validation.
    // ------------------------------------------------------------

    {
        const DatasetPair pair =
            makePair("040001");

        FakeImageDecoder decoder;

        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {
                std::numeric_limits<
                    double
                >::quiet_NaN()
            }
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        try {
            static_cast<void>(
                batchValidator.validatePairs(
                    root,
                    {pair}
                )
            );

            return 31;
        }
        catch (const std::runtime_error&) {
        }

        if (!decoder.calls().empty()) {
            return 32;
        }
    }

    // ------------------------------------------------------------
    // Backward clock after a completed pair.
    // ------------------------------------------------------------

    {
        const DatasetPair pair =
            makePair("050001");

        const auto visiblePath =
            root / "visible/050001.jpg";

        const auto thermalPath =
            root / "thermal/050001.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded()
        );

        decoder.setResult(
            thermalPath,
            decoded()
        );

        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {
                10.0,
                11.0,
                10.5
            }
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        RecordingProgressObserver observer;

        try {
            static_cast<void>(
                batchValidator.validatePairs(
                    root,
                    {pair},
                    std::ref(observer)
                )
            );

            return 33;
        }
        catch (const std::runtime_error&) {
        }

        if (!observer.observations().empty()) {
            return 34;
        }
    }

    // ------------------------------------------------------------
    // Non-finite pair stop after successful validation.
    // ------------------------------------------------------------

    {
        const DatasetPair pair =
            makePair("060001");

        const auto visiblePath =
            root / "visible/060001.jpg";

        const auto thermalPath =
            root / "thermal/060001.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded()
        );

        decoder.setResult(
            thermalPath,
            decoded()
        );

        PairValidator pairValidator(
            decoder,
            policy()
        );

        SequenceClock clock(
            {
                0.0,
                1.0,
                std::numeric_limits<
                    double
                >::infinity()
            }
        );

        BatchPairValidator batchValidator(
            pairValidator,
            std::ref(clock)
        );

        try {
            static_cast<void>(
                batchValidator.validatePairs(
                    root,
                    {pair}
                )
            );

            return 35;
        }
        catch (const std::runtime_error&) {
        }

        if (decoder.calls().size() != 2) {
            return 36;
        }
    }

    return 0;
}