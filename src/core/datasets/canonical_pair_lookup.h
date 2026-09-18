#ifndef QART_CORE_DATASETS_CANONICAL_PAIR_LOOKUP_H
#define QART_CORE_DATASETS_CANONICAL_PAIR_LOOKUP_H

#pragma once

#include "core/domain/datasets/dataset_pair.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace qart::core::datasets {

class CanonicalPairLookup final
{
public:
    [[nodiscard]]
    static std::optional<domain::datasets::DatasetPair> findByImagePath(
        const std::filesystem::path& datasetRoot,
        const std::filesystem::path& imagePath,
        const std::vector<domain::datasets::DatasetPair>& canonicalPairs
    );
};

} // namespace qart::core::datasets

#endif // QART_CORE_DATASETS_CANONICAL_PAIR_LOOKUP_H
