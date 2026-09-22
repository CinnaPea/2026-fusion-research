//
// Created by hakgu on 8/15/2026.
//

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/imaging/image_decoder.h"

#ifndef VISUAL_THERMAL_CONCEPT_PAIR_VALIDATOR_H
#define VISUAL_THERMAL_CONCEPT_PAIR_VALIDATOR_H

namespace qart::core::validation {

    class PairValidator final
    {
    public:
        PairValidator(
            imaging::ImageDecoder& decoder,
            domain::validation::PairValidationPolicy policy
        );

        [[nodiscard]]
        domain::validation::PairValidationResult validatePair(
            const std::filesystem::path& datasetRoot,
            const domain::datasets::DatasetPair& pair
        );

    private:
        [[nodiscard]]
        static std::filesystem::path resolvePhysicalPath(
            const std::filesystem::path& datasetRoot,
            const std::string& relativePath
        );

        [[nodiscard]]
        static bool extensionIsSupported(
            const std::filesystem::path& imagePath,
            const std::vector<std::string>& supportedExtensions
        );

        [[nodiscard]]
        std::optional<domain::imaging::ImageDecodeResult>
        decodeWhenEligible(
            const std::filesystem::path& imagePath,
            bool fileExists,
            bool extensionSupported
        );

        imaging::ImageDecoder& decoder_;
        domain::validation::PairValidationPolicy policy_;
    };

}

#endif //VISUAL_THERMAL_CONCEPT_PAIR_VALIDATOR_H