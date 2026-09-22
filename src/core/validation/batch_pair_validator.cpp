//
// Created by hakgu on 8/17/2026.
//

#include "core/validation/batch_pair_validator.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/domain/validation/validation_status_count.h"

#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace qart::core::validation {

namespace {

using domain::validation::PairValidationStatus;

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

} // namespace

BatchPairValidator::BatchPairValidator(
    PairValidator& pairValidator,
    Clock clock
)
    : pairValidator_(pairValidator),
      clock_(
          clock
              ? std::move(clock)
              : Clock(&BatchPairValidator::defaultClock)
      )
{
}

double BatchPairValidator::defaultClock() noexcept
{
    using ClockType =
        std::chrono::steady_clock;

    const auto now =
        ClockType::now().time_since_epoch();

    return std::chrono::duration<double>(
        now
    ).count();
}

void BatchPairValidator::rejectDuplicatePairIds(
    const std::vector<domain::datasets::DatasetPair>& pairs
)
{
    std::set<std::string> observedPairIds;
    std::set<std::string> duplicatePairIds;

    for (const domain::datasets::DatasetPair& pair : pairs) {
        const auto insertion =
            observedPairIds.insert(
                pair.pairId()
            );

        if (!insertion.second) {
            duplicatePairIds.insert(
                pair.pairId()
            );
        }
    }

    if (duplicatePairIds.empty()) {
        return;
    }

    std::string formattedDuplicates;

    for (const std::string& duplicate : duplicatePairIds) {
        if (!formattedDuplicates.empty()) {
            formattedDuplicates += ", ";
        }

        formattedDuplicates += duplicate;
    }

    throw domain::datasets::DatasetPairingError(
        "Batch contains duplicate pair identifiers: "
        + formattedDuplicates
    );
}

domain::validation::BatchValidationSummary
BatchPairValidator::buildSummary(
    const std::vector<
        domain::validation::PairValidationResult
    >& results
)
{
    using domain::validation::BatchValidationSummary;
    using domain::validation::ValidationStatusCount;

    std::map<PairValidationStatus, int> counts;

    for (const PairValidationStatus status : allStatuses) {
        counts.emplace(
            status,
            0
        );
    }

    for (
        const domain::validation::PairValidationResult& result
        : results
    ) {
        ++counts[result.status()];
    }

    std::vector<ValidationStatusCount>
        statusCounts;

    statusCounts.reserve(
        allStatuses.size()
    );

    for (const PairValidationStatus status : allStatuses) {
        statusCounts.emplace_back(
            status,
            counts.at(status)
        );
    }

    const int validPairs =
        counts.at(
            PairValidationStatus::Valid
        );

    const int totalPairs =
        static_cast<int>(
            results.size()
        );

    return BatchValidationSummary(
        totalPairs,
        validPairs,
        totalPairs - validPairs,
        std::move(statusCounts)
    );
}

domain::validation::BatchValidationReport
BatchPairValidator::validatePairs(
    const std::filesystem::path& datasetRoot,
    const std::vector<domain::datasets::DatasetPair>& pairs,
    const ProgressObserver& progressObserver
)
{
    using domain::validation::BatchValidationProgress;
    using domain::validation::BatchValidationReport;
    using domain::validation::PairValidationResult;

    rejectDuplicatePairIds(
        pairs
    );

    if (pairs.empty()) {
        std::vector<PairValidationResult>
            emptyResults;

        return BatchValidationReport(
            emptyResults,
            buildSummary(emptyResults)
        );
    }

    const double batchStartedAt =
        clock_();

    if (!std::isfinite(batchStartedAt)) {
        throw std::runtime_error(
            "Batch progress clock returned a non-finite start value."
        );
    }

    std::vector<PairValidationResult>
        completedResults;

    completedResults.reserve(
        pairs.size()
    );

    const int totalPairs =
        static_cast<int>(
            pairs.size()
        );

    int completedPairs = 0;

    for (const domain::datasets::DatasetPair& pair : pairs) {
        ++completedPairs;

        const double pairStartedAt =
            clock_();

        PairValidationResult result =
            pairValidator_.validatePair(
                datasetRoot,
                pair
            );

        const double pairStoppedAt =
            clock_();

        if (!std::isfinite(pairStartedAt)) {
            throw std::runtime_error(
                "Batch progress clock returned a non-finite "
                "pair-start value."
            );
        }

        if (!std::isfinite(pairStoppedAt)) {
            throw std::runtime_error(
                "Batch progress clock returned a non-finite "
                "pair-stop value."
            );
        }

        const double pairElapsedSeconds =
            pairStoppedAt
            - pairStartedAt;

        const double batchElapsedSeconds =
            pairStoppedAt
            - batchStartedAt;

        if (
            pairElapsedSeconds < 0.0
            || batchElapsedSeconds < 0.0
        ) {
            throw std::runtime_error(
                "Batch progress clock moved backward."
            );
        }

        completedResults.push_back(
            result
        );

        if (progressObserver) {
            progressObserver(
                BatchValidationProgress(
                    completedPairs,
                    totalPairs,
                    result,
                    pairElapsedSeconds,
                    batchElapsedSeconds
                )
            );
        }
    }

    BatchValidationReport report(
        completedResults,
        buildSummary(completedResults)
    );

    return report;
}

} // namespace qart::core::validation