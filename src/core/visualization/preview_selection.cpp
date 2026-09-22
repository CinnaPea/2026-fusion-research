//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/preview_selection.h"

#include "core/domain/validation/pair_validation_status.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace qart::core::visualization {

namespace {

bool isBlank(
    const std::string& value
) noexcept
{
    if (value.empty()) {
        return true;
    }

    for (const unsigned char character : value) {
        if (!std::isspace(character)) {
            return false;
        }
    }

    return true;
}

bool hasSurroundingWhitespace(
    const std::string& value
) noexcept
{
    if (value.empty()) {
        return false;
    }

    return
        std::isspace(
            static_cast<unsigned char>(
                value.front()
            )
        )
        || std::isspace(
            static_cast<unsigned char>(
                value.back()
            )
        );
}

} // namespace


PreviewSelectionRequest::PreviewSelectionRequest(
    const int automaticCountPerPartition,
    std::vector<std::string> explicitPairIds
)
    : automaticCountPerPartition_(
          automaticCountPerPartition
      ),
      explicitPairIds_(
          std::move(explicitPairIds)
      )
{
    validate();
}

void PreviewSelectionRequest::validate() const
{
    if (
        automaticCountPerPartition_
        < 2
    ) {
        throw std::invalid_argument(
            "automaticCountPerPartition "
            "must be at least two."
        );
    }

    for (
        const std::string& pairId
        : explicitPairIds_
    ) {
        if (isBlank(pairId)) {
            throw std::invalid_argument(
                "Explicit preview pair IDs "
                "must not be blank."
            );
        }

        if (
            hasSurroundingWhitespace(
                pairId
            )
        ) {
            throw std::invalid_argument(
                "Explicit preview pair IDs must not "
                "contain leading or trailing whitespace."
            );
        }
    }
}

int
PreviewSelectionRequest::
automaticCountPerPartition() const noexcept
{
    return automaticCountPerPartition_;
}

const std::vector<std::string>&
PreviewSelectionRequest::
explicitPairIds() const noexcept
{
    return explicitPairIds_;
}


PreviewSelection::PreviewSelection(
    std::vector<
        domain::validation::PairValidationResult
    > automaticResults,
    std::vector<
        domain::validation::PairValidationResult
    > explicitResults
)
    : automaticResults_(
          std::move(automaticResults)
      ),
      explicitResults_(
          std::move(explicitResults)
      )
{
    validate();
}

void PreviewSelection::validate() const
{
    std::unordered_set<std::string>
        pairIds;

    const auto validateResults =
        [&pairIds](
            const std::vector<
                domain::validation::PairValidationResult
            >& results
        ) {
            for (
                const auto& result
                : results
            ) {
                if (
                    result.status()
                    != domain::validation::
                        PairValidationStatus::Valid
                ) {
                    throw std::invalid_argument(
                        "Preview selection may contain "
                        "only VALID results: "
                        + result.pair().pairId()
                    );
                }

                const auto insertion =
                    pairIds.insert(
                        result.pair().pairId()
                    );

                if (!insertion.second) {
                    throw std::invalid_argument(
                        "Preview selection must contain "
                        "unique pair IDs."
                    );
                }
            }
        };

    validateResults(
        automaticResults_
    );

    validateResults(
        explicitResults_
    );
}

const std::vector<
    domain::validation::PairValidationResult
>&
PreviewSelection::
automaticResults() const noexcept
{
    return automaticResults_;
}

const std::vector<
    domain::validation::PairValidationResult
>&
PreviewSelection::
explicitResults() const noexcept
{
    return explicitResults_;
}

std::vector<
    domain::validation::PairValidationResult
>
PreviewSelection::allResults() const
{
    std::vector<
        domain::validation::PairValidationResult
    > combined;

    combined.reserve(
        totalSelected()
    );

    combined.insert(
        combined.end(),
        automaticResults_.begin(),
        automaticResults_.end()
    );

    combined.insert(
        combined.end(),
        explicitResults_.begin(),
        explicitResults_.end()
    );

    return combined;
}

std::size_t
PreviewSelection::totalSelected() const noexcept
{
    return
        automaticResults_.size()
        + explicitResults_.size();
}

} // namespace qart::core::visualization