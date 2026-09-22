//
// Created by hakgu on 8/15/2026.
//

#include "core/validation/pair_validator.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_decode_result.h"
#include "core/domain/imaging/image_decode_status.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/imaging/image_decoder.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
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
using qart::core::domain::validation::PairValidationPolicy;
using qart::core::domain::validation::PairValidationStatus;
using qart::core::imaging::ImageDecoder;
using qart::core::validation::PairValidator;

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

    ImageDecodeResult decode(
        const std::filesystem::path& imagePath
    ) override
    {
        calls_.push_back(imagePath);

        const auto result =
            results_.find(imagePath);

        if (result == results_.end()) {
            return ImageDecodeResult(
                ImageDecodeStatus::DecodeFailed,
                std::nullopt,
                "Fake decoder has no configured result."
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

    std::vector<std::filesystem::path> calls_;
};

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
            std::filesystem::temp_directory_path()
            / "qart_pair_validator_contract"
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
    const std::string& pairId,
    const std::string& visiblePath =
        "visible/sample.jpg",
    const std::string& thermalPath =
        "thermal/sample.jpg"
)
{
    return DatasetPair(
        pairId,
        "fixture",
        DatasetPartitionId("test"),
        "sample",
        visiblePath,
        thermalPath
    );
}

std::filesystem::path createCaseRoot(
    const std::filesystem::path& base,
    const std::string& name
)
{
    const std::filesystem::path root =
        base / name;

    std::filesystem::create_directories(
        root / "visible"
    );

    std::filesystem::create_directories(
        root / "thermal"
    );

    return root;
}

void createFile(
    const std::filesystem::path& path
)
{
    std::filesystem::create_directories(
        path.parent_path()
    );

    std::ofstream file(
        path,
        std::ios::binary
    );

    file << "fixture";
}

ImageDecodeResult decoded(
    const int width,
    const int height
)
{
    return ImageDecodeResult(
        ImageDecodeStatus::Decoded,
        ImageDimensions(width, height)
    );
}

ImageDecodeResult failed(
    const std::string& message
)
{
    return ImageDecodeResult(
        ImageDecodeStatus::DecodeFailed,
        std::nullopt,
        message
    );
}

PairValidationPolicy defaultPolicy()
{
    return PairValidationPolicy(
        {".jpg", ".png"},
        {".jpg", ".png"}
    );
}

} // namespace

int main()
{
    TemporaryDirectory temporaryDirectory;

    const std::filesystem::path base =
        temporaryDirectory.path();

    // ------------------------------------------------------------
    // Invalid dataset root: missing
    // ------------------------------------------------------------

    {
        FakeImageDecoder decoder;
        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        try {
            static_cast<void>(
                validator.validatePair(
                    base / "missing-root",
                    makePair("missing_root")
                )
            );

            return 1;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Invalid dataset root: not a directory
    // ------------------------------------------------------------

    {
        const std::filesystem::path rootFile =
            base / "root-file";

        createFile(rootFile);

        FakeImageDecoder decoder;
        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        try {
            static_cast<void>(
                validator.validatePair(
                    rootFile,
                    makePair("root_file")
                )
            );

            return 2;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Both sources absent => fatal pairing error
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "both_missing"
            );

        FakeImageDecoder decoder;
        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        try {
            static_cast<void>(
                validator.validatePair(
                    root,
                    makePair("both_missing")
                )
            );

            return 3;
        }
        catch (const DatasetPairingError&) {
        }

        if (!decoder.calls().empty()) {
            return 4;
        }
    }

    // ------------------------------------------------------------
    // Missing visible
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "missing_visible"
            );

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            thermalPath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair("missing_visible")
            );

        if (
            result.status()
            != PairValidationStatus::MissingVisible
        ) {
            return 5;
        }

        if (
            result.visibleDecoded()
            || !result.thermalDecoded()
        ) {
            return 6;
        }
    }

    // ------------------------------------------------------------
    // Missing thermal
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "missing_thermal"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        createFile(visiblePath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair("missing_thermal")
            );

        if (
            result.status()
            != PairValidationStatus::MissingThermal
        ) {
            return 7;
        }
    }

    // ------------------------------------------------------------
    // Unsupported visible format
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "unsupported_visible"
            );

        const auto visiblePath =
            root / "visible/sample.txt";

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            thermalPath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "unsupported_visible",
                    "visible/sample.txt",
                    "thermal/sample.jpg"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                UnsupportedVisibleFormat
        ) {
            return 8;
        }

        if (
            decoder.calls().size() != 1
            || decoder.calls().front()
                != thermalPath
        ) {
            return 9;
        }
    }

    // ------------------------------------------------------------
    // Unsupported thermal format
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "unsupported_thermal"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        const auto thermalPath =
            root / "thermal/sample.txt";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "unsupported_thermal",
                    "visible/sample.jpg",
                    "thermal/sample.txt"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                UnsupportedThermalFormat
        ) {
            return 10;
        }

        if (
            decoder.calls().size() != 1
            || decoder.calls().front()
                != visiblePath
        ) {
            return 11;
        }
    }

    // ------------------------------------------------------------
    // Visible decode failure
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "visible_decode_failed"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            failed("visible decoder failure")
        );

        decoder.setResult(
            thermalPath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "visible_decode_failed"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                VisibleDecodeFailed
        ) {
            return 12;
        }

        if (
            !result.message().has_value()
            || result.message().value()
                != "visible decoder failure"
        ) {
            return 13;
        }
    }

    // ------------------------------------------------------------
    // Thermal decode failure
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "thermal_decode_failed"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        decoder.setResult(
            thermalPath,
            failed("thermal decoder failure")
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "thermal_decode_failed"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                ThermalDecodeFailed
        ) {
            return 14;
        }
    }

    // ------------------------------------------------------------
    // Pairwise dimensions disagree
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "dimension_mismatch"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        decoder.setResult(
            thermalPath,
            decoded(320, 240)
        );

        PairValidator validator(
            decoder,
            PairValidationPolicy(
                {".jpg"},
                {".jpg"},
                ImageDimensions(1280, 1024),
                ImageDimensions(1280, 1024)
            )
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "dimension_mismatch"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                DimensionMismatch
        ) {
            return 15;
        }
    }

    // ------------------------------------------------------------
    // Visible expectation outranks thermal expectation
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "unexpected_visible"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        decoder.setResult(
            thermalPath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            PairValidationPolicy(
                {".jpg"},
                {".jpg"},
                ImageDimensions(1280, 1024),
                ImageDimensions(1280, 1024)
            )
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "unexpected_visible"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                UnexpectedVisibleDimensions
        ) {
            return 16;
        }
    }

    // ------------------------------------------------------------
    // Thermal expectation only
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "unexpected_thermal"
            );

        const auto visiblePath =
            root / "visible/sample.jpg";

        const auto thermalPath =
            root / "thermal/sample.jpg";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        decoder.setResult(
            thermalPath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            PairValidationPolicy(
                {".jpg"},
                {".jpg"},
                ImageDimensions(640, 480),
                ImageDimensions(1280, 1024)
            )
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "unexpected_thermal"
                )
            );

        if (
            result.status()
            != PairValidationStatus::
                UnexpectedThermalDimensions
        ) {
            return 17;
        }
    }

    // ------------------------------------------------------------
    // VALID + case-insensitive extension check
    // ------------------------------------------------------------

    {
        const auto root =
            createCaseRoot(
                base,
                "valid"
            );

        const auto visiblePath =
            root / "visible/sample.JPG";

        const auto thermalPath =
            root / "thermal/sample.PNG";

        createFile(visiblePath);
        createFile(thermalPath);

        FakeImageDecoder decoder;

        decoder.setResult(
            visiblePath,
            decoded(640, 480)
        );

        decoder.setResult(
            thermalPath,
            decoded(640, 480)
        );

        PairValidator validator(
            decoder,
            defaultPolicy()
        );

        const auto result =
            validator.validatePair(
                root,
                makePair(
                    "valid",
                    "visible/sample.JPG",
                    "thermal/sample.PNG"
                )
            );

        if (!result.isValid()) {
            return 18;
        }

        if (
            result.status()
            != PairValidationStatus::Valid
        ) {
            return 19;
        }

        if (!result.dimensionsMatch()) {
            return 20;
        }

        if (decoder.calls().size() != 2) {
            return 21;
        }
    }

    return 0;
}