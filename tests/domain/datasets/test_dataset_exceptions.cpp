//
// Created by hakgu on 8/14/2026.
//

#include "core/domain/datasets/dataset_exceptions.h"

#include <exception>
#include <string>
#include <type_traits>

namespace {

    using qart::core::domain::datasets::DatasetConfigurationError;
    using qart::core::domain::datasets::DatasetError;
    using qart::core::domain::datasets::DatasetLayoutError;
    using qart::core::domain::datasets::DatasetManifestError;
    using qart::core::domain::datasets::DatasetPairingError;

} // namespace

int main()
{
    static_assert(
        std::is_base_of_v<std::exception, DatasetError>,
        "DatasetError must be a standard C++ exception."
    );

    static_assert(
        std::is_base_of_v<
            DatasetError,
            DatasetConfigurationError
        >
    );

    static_assert(
        std::is_base_of_v<
            DatasetError,
            DatasetLayoutError
        >
    );

    static_assert(
        std::is_base_of_v<
            DatasetError,
            DatasetPairingError
        >
    );

    static_assert(
        std::is_base_of_v<
            DatasetError,
            DatasetManifestError
        >
    );

    try {
        throw DatasetLayoutError(
            "Shared dataset root does not exist."
        );
    }
    catch (const DatasetError& error) {
        if (
            std::string(error.what())
            != "Shared dataset root does not exist."
        ) {
            return 1;
        }
    }
    catch (...) {
        return 2;
    }

    try {
        throw DatasetPairingError(
            "Candidate pair references no source files."
        );
    }
    catch (const DatasetPairingError& error) {
        if (
            std::string(error.what())
            != "Candidate pair references no source files."
        ) {
            return 3;
        }
    }
    catch (...) {
        return 4;
    }

    return 0;
}