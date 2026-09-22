#include "core/datasets/canonical_pair_lookup.h"

#include <system_error>

namespace qart::core::datasets {

std::optional<domain::datasets::DatasetPair>
CanonicalPairLookup::findByImagePath(
    const std::filesystem::path& datasetRoot,
    const std::filesystem::path& imagePath,
    const std::vector<domain::datasets::DatasetPair>& canonicalPairs
)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(datasetRoot, error);
    if (error) {
        return std::nullopt;
    }

    const auto canonicalImage = std::filesystem::weakly_canonical(imagePath, error);
    if (error) {
        return std::nullopt;
    }

    const auto relativePath = std::filesystem::relative(canonicalImage, canonicalRoot, error);
    if (error || relativePath.empty() || relativePath.is_absolute()) {
        return std::nullopt;
    }

    const std::string relative = relativePath.generic_string();
    if (relative == ".." || relative.rfind("../", 0) == 0) {
        return std::nullopt;
    }

    for (const auto& pair : canonicalPairs) {
        if (
            pair.visibleRelativePath() == relative
            || pair.thermalRelativePath() == relative
        ) {
            return pair;
        }
    }

    return std::nullopt;
}

} // namespace qart::core::datasets
