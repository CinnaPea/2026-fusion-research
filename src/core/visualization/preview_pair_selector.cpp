//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/preview_pair_selector.h"

#include "core/domain/validation/pair_validation_status.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qart::core::visualization {

namespace {

using domain::validation::PairValidationResult;
using domain::validation::PairValidationStatus;

} // namespace


std::vector<std::size_t>
PreviewPairSelector::evenlySpacedIndices(
    const std::size_t availableCount,
    const int requestedCount
)
{
    if (requestedCount < 2) {
        throw std::invalid_argument(
            "requestedCount must be at least two."
        );
    }

    if (availableCount == 0) {
        return {};
    }

    const std::size_t requested =
        static_cast<std::size_t>(
            requestedCount
        );

    if (availableCount <= requested) {
        std::vector<std::size_t> indices;

        indices.reserve(
            availableCount
        );

        for (
            std::size_t index = 0;
            index < availableCount;
            ++index
        ) {
            indices.push_back(
                index
            );
        }

        return indices;
    }

    const std::size_t denominator =
        requested - 1;

    const std::size_t finalIndex =
        availableCount - 1;

    std::vector<std::size_t> indices;

    indices.reserve(
        requested
    );

    for (
        std::size_t index = 0;
        index < requested;
        ++index
    ) {
        indices.push_back(
            index
            * finalIndex
            / denominator
        );
    }

    return indices;
}


std::vector<std::string>
PreviewPairSelector::deduplicatePairIds(
    const std::vector<std::string>& pairIds
)
{
    std::unordered_set<std::string>
        observedPairIds;

    std::vector<std::string>
        uniquePairIds;

    uniquePairIds.reserve(
        pairIds.size()
    );

    for (
        const std::string& pairId
        : pairIds
    ) {
        const auto insertion =
            observedPairIds.insert(
                pairId
            );

        if (!insertion.second) {
            continue;
        }

        uniquePairIds.push_back(
            pairId
        );
    }

    return uniquePairIds;
}


std::vector<
    PreviewPairSelector::PartitionGroup
>
PreviewPairSelector::
groupValidResultsByPartition(
    const std::vector<
        PairValidationResult
    >& results
)
{
    std::vector<PartitionGroup>
        groups;

    std::unordered_map<
        std::string,
        std::size_t
    > groupIndexByPartition;

    for (
        const PairValidationResult& result
        : results
    ) {
        const std::string partitionId =
            result.pair()
                .partitionId()
                .value();

        auto groupIterator =
            groupIndexByPartition.find(
                partitionId
            );

        if (
            groupIterator
            == groupIndexByPartition.end()
        ) {
            const std::size_t newIndex =
                groups.size();

            groups.push_back(
                PartitionGroup{
                    partitionId,
                    {}
                }
            );

            groupIndexByPartition.emplace(
                partitionId,
                newIndex
            );

            groupIterator =
                groupIndexByPartition.find(
                    partitionId
                );
        }

        if (
            result.status()
            == PairValidationStatus::Valid
        ) {
            groups
                .at(groupIterator->second)
                .validResults
                .push_back(
                    &result
                );
        }
    }

    return groups;
}


std::vector<PairValidationResult>
PreviewPairSelector::
selectAutomaticResults(
    const std::vector<
        PairValidationResult
    >& results,
    const int countPerPartition
)
{
    const auto groups =
        groupValidResultsByPartition(
            results
        );

    std::vector<PairValidationResult>
        selectedResults;

    for (
        const PartitionGroup& group
        : groups
    ) {
        const auto indices =
            evenlySpacedIndices(
                group.validResults.size(),
                countPerPartition
            );

        for (
            const std::size_t index
            : indices
        ) {
            selectedResults.push_back(
                *group.validResults.at(
                    index
                )
            );
        }
    }

    return selectedResults;
}


PreviewSelection
PreviewPairSelector::select(
    const std::vector<
        PairValidationResult
    >& results,
    const PreviewSelectionRequest& request
) const
{
    std::unordered_map<
        std::string,
        const PairValidationResult*
    > resultByPairId;

    resultByPairId.reserve(
        results.size()
    );

    for (
        const PairValidationResult& result
        : results
    ) {
        const std::string& pairId =
            result.pair().pairId();

        const auto insertion =
            resultByPairId.emplace(
                pairId,
                &result
            );

        if (!insertion.second) {
            throw PreviewSelectionError(
                "Preview candidates contain "
                "duplicate pair ID: '"
                + pairId
                + "'."
            );
        }
    }

    std::vector<PairValidationResult>
        automaticResults =
            selectAutomaticResults(
                results,
                request
                    .automaticCountPerPartition()
            );

    const std::vector<std::string>
        uniqueExplicitPairIds =
            deduplicatePairIds(
                request.explicitPairIds()
            );

    std::vector<std::string>
        missingPairIds;

    std::vector<
        const PairValidationResult*
    > nonvalidResults;

    std::vector<
        const PairValidationResult*
    > resolvedExplicitResults;

    for (
        const std::string& pairId
        : uniqueExplicitPairIds
    ) {
        const auto resultIterator =
            resultByPairId.find(
                pairId
            );

        if (
            resultIterator
            == resultByPairId.end()
        ) {
            missingPairIds.push_back(
                pairId
            );

            continue;
        }

        const PairValidationResult* result =
            resultIterator->second;

        if (
            result->status()
            != PairValidationStatus::Valid
        ) {
            nonvalidResults.push_back(
                result
            );

            continue;
        }

        resolvedExplicitResults.push_back(
            result
        );
    }

    if (
        !missingPairIds.empty()
        || !nonvalidResults.empty()
    ) {
        std::string message =
            "Explicit preview selection failed: ";

        bool hasPreviousDetail = false;

        if (!missingPairIds.empty()) {
            message += "missing IDs=[";

            for (
                std::size_t index = 0;
                index < missingPairIds.size();
                ++index
            ) {
                if (index != 0) {
                    message += ", ";
                }

                message +=
                    missingPairIds[index];
            }

            message += "]";

            hasPreviousDetail = true;
        }

        if (!nonvalidResults.empty()) {
            if (hasPreviousDetail) {
                message += "; ";
            }

            message +=
                "non-VALID IDs=[";

            for (
                std::size_t index = 0;
                index < nonvalidResults.size();
                ++index
            ) {
                if (index != 0) {
                    message += ", ";
                }

                const auto* result =
                    nonvalidResults[index];

                message +=
                    result->pair().pairId()
                    + ":"
                    + std::string(
                        domain::validation::toString(
                            result->status()
                        )
                    );
            }

            message += "]";
        }

        message += ".";

        throw PreviewSelectionError(
            message
        );
    }

    std::unordered_set<std::string>
        automaticPairIds;

    automaticPairIds.reserve(
        automaticResults.size()
    );

    for (
        const PairValidationResult& result
        : automaticResults
    ) {
        automaticPairIds.insert(
            result.pair().pairId()
        );
    }

    std::vector<PairValidationResult>
        explicitResults;

    explicitResults.reserve(
        resolvedExplicitResults.size()
    );

    for (
        const PairValidationResult* result
        : resolvedExplicitResults
    ) {
        if (
            automaticPairIds.find(
                result->pair().pairId()
            )
            != automaticPairIds.end()
        ) {
            continue;
        }

        explicitResults.push_back(
            *result
        );
    }

    return PreviewSelection(
        std::move(
            automaticResults
        ),
        std::move(
            explicitResults
        )
    );
}

} // namespace qart::core::visualization