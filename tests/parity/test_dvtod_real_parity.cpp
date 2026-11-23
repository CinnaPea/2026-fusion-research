#include "core/datasets/dvtod_adapter.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/manifests/csv_validation_manifest_reader.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

using qart::core::datasets::DVTODAdapter;
using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::imaging::ImageDimensions;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;
using qart::core::manifests::CsvValidationManifestReader;

constexpr std::size_t kExpectedTotal = 2179;
constexpr std::size_t kExpectedTrain = 1606;
constexpr std::size_t kExpectedVal = 573;

int fail(const int code, const std::string& message)
{
    std::cerr << "[FAIL] " << message << '\n';
    return code;
}

bool pairsEqual(const DatasetPair& left, const DatasetPair& right)
{
    return
        left.pairId() == right.pairId()
        && left.datasetId() == right.datasetId()
        && left.partitionId() == right.partitionId()
        && left.sourceStem() == right.sourceStem()
        && left.visibleRelativePath() == right.visibleRelativePath()
        && left.thermalRelativePath() == right.thermalRelativePath();
}

std::vector<std::size_t>
evenlySpacedIndices(
    const std::size_t availableCount,
    const std::size_t requestedCount
)
{
    std::vector<std::size_t> indices;

    if (availableCount == 0 || requestedCount == 0) {
        return indices;
    }

    if (availableCount <= requestedCount) {
        for (std::size_t index = 0; index < availableCount; ++index) {
            indices.push_back(index);
        }
        return indices;
    }

    const std::size_t denominator = requestedCount - 1;
    const std::size_t finalIndex = availableCount - 1;

    for (std::size_t index = 0; index < requestedCount; ++index) {
        indices.push_back(index * finalIndex / denominator);
    }

    return indices;
}

bool verifyAuditSample(
    const std::vector<PairValidationResult>& results,
    const std::vector<std::string>& expectedIds
)
{
    const auto indices =
        evenlySpacedIndices(results.size(), expectedIds.size());

    if (indices.size() != expectedIds.size()) {
        return false;
    }

    for (std::size_t index = 0; index < expectedIds.size(); ++index) {
        if (
            results.at(indices.at(index)).pair().pairId()
            != expectedIds.at(index)
        ) {
            return false;
        }
    }

    return true;
}

} // namespace

int main(const int argc, char* argv[])
{
    if (argc != 2) {
        return fail(
            1,
            "Usage: test_dvtod_real_parity <absolute QART dataset root>"
        );
    }

    const std::filesystem::path datasetRoot =
        std::filesystem::absolute(std::filesystem::path(argv[1]));

    const std::filesystem::path manifestPath =
        datasetRoot
        / "manifests"
        / "dvtod"
        / "dvtod_validation_schema_2_0.csv";

    if (!std::filesystem::is_directory(datasetRoot)) {
        return fail(2, "Dataset root is not a directory.");
    }

    if (!std::filesystem::is_regular_file(manifestPath)) {
        return fail(
            3,
            "Frozen DVTOD Schema 2.0 manifest was not found: "
            + manifestPath.string()
        );
    }

    DVTODAdapter adapter;

    const auto trainPairs =
        adapter.discoverPairs(datasetRoot, DatasetPartitionId("train"));

    const auto valPairs =
        adapter.discoverPairs(datasetRoot, DatasetPartitionId("val"));

    if (trainPairs.size() != kExpectedTrain) {
        return fail(
            4,
            "Expected 1606 DVTOD train pairs but found "
            + std::to_string(trainPairs.size())
        );
    }

    if (valPairs.size() != kExpectedVal) {
        return fail(
            5,
            "Expected 573 DVTOD val pairs but found "
            + std::to_string(valPairs.size())
        );
    }

    if (
        trainPairs.front().pairId() != "dvtod_train_1"
        || trainPairs.back().pairId() != "dvtod_train_2179"
    ) {
        return fail(
            6,
            "DVTOD train first/last identity parity failed."
        );
    }

    if (
        valPairs.front().pairId() != "dvtod_val_3"
        || valPairs.back().pairId() != "dvtod_val_2180"
    ) {
        return fail(
            7,
            "DVTOD val first/last identity parity failed."
        );
    }

    std::vector<DatasetPair> discoveredPairs;
    discoveredPairs.reserve(kExpectedTotal);

    discoveredPairs.insert(
        discoveredPairs.end(),
        trainPairs.begin(),
        trainPairs.end()
    );

    discoveredPairs.insert(
        discoveredPairs.end(),
        valPairs.begin(),
        valPairs.end()
    );

    CsvValidationManifestReader reader;
    const auto manifest = reader.read(manifestPath);

    if (manifest.schemaVersion() != "2.0") {
        return fail(8, "Expected DVTOD Schema 2.0.");
    }

    if (manifest.rows().size() != kExpectedTotal) {
        return fail(9, "DVTOD manifest row count is not 2179.");
    }

    const auto manifestResults = manifest.toResults();

    if (manifestResults.size() != kExpectedTotal) {
        return fail(
            10,
            "DVTOD manifest did not reconstruct 2179 typed results."
        );
    }

    for (std::size_t index = 0; index < kExpectedTotal; ++index) {
        if (
            !pairsEqual(
                discoveredPairs.at(index),
                manifestResults.at(index).pair()
            )
        ) {
            return fail(
                11,
                "Ordered DVTOD DatasetPair parity failed at index "
                + std::to_string(index)
            );
        }
    }

    const ImageDimensions expectedVisible(1920, 1080);
    const ImageDimensions expectedThermal(640, 512);

    std::vector<PairValidationResult> trainResults;
    std::vector<PairValidationResult> valResults;

    trainResults.reserve(kExpectedTrain);
    valResults.reserve(kExpectedVal);

    for (const PairValidationResult& result : manifestResults) {
        if (
            result.status()
            != PairValidationStatus::DimensionMismatch
        ) {
            return fail(
                12,
                "DVTOD row is not DIMENSION_MISMATCH: "
                + result.pair().pairId()
            );
        }

        if (!result.sourcePairValid()) {
            return fail(
                13,
                "DVTOD mismatch pair is not source-pair valid: "
                + result.pair().pairId()
            );
        }

        if (result.isValid()) {
            return fail(
                14,
                "DVTOD raw pair must not be same-geometry VALID: "
                + result.pair().pairId()
            );
        }

        if (!result.visibleDecoded() || !result.thermalDecoded()) {
            return fail(
                15,
                "DVTOD source pair does not report both modalities decoded."
            );
        }

        if (
            !result.visibleDimensions().has_value()
            || !result.thermalDimensions().has_value()
        ) {
            return fail(
                16,
                "DVTOD source pair lacks dimensions."
            );
        }

        if (result.visibleDimensions().value() != expectedVisible) {
            return fail(
                17,
                "DVTOD visible geometry is not 1920x1080."
            );
        }

        if (result.thermalDimensions().value() != expectedThermal) {
            return fail(
                18,
                "DVTOD thermal geometry is not 640x512."
            );
        }

        if (result.dimensionsMatch()) {
            return fail(
                19,
                "Raw DVTOD pair unexpectedly reports matching geometry."
            );
        }

        if (
            result.pair().visibleRelativePath().find(
                "raw/dvtdo/vis1/images/"
            ) != 0
        ) {
            return fail(
                20,
                "Non-canonical DVTOD visible path."
            );
        }

        if (
            result.pair().thermalRelativePath().find(
                "raw/dvtdo/ir1/images/"
            ) != 0
        ) {
            return fail(
                21,
                "Non-canonical DVTOD thermal path."
            );
        }

        if (
            result.pair().partitionId()
            == DatasetPartitionId("train")
        ) {
            trainResults.push_back(result);
        }
        else if (
            result.pair().partitionId()
            == DatasetPartitionId("val")
        ) {
            valResults.push_back(result);
        }
        else {
            return fail(
                22,
                "Unexpected DVTOD partition."
            );
        }
    }

    if (
        trainResults.size() != kExpectedTrain
        || valResults.size() != kExpectedVal
    ) {
        return fail(
            23,
            "Manifest partition counts differ from 1606 train / 573 val."
        );
    }

    const std::vector<std::string> expectedTrainAuditIds = {
        "dvtod_train_1",
        "dvtod_train_551",
        "dvtod_train_1119",
        "dvtod_train_1673",
        "dvtod_train_2179"
    };

    const std::vector<std::string> expectedValAuditIds = {
        "dvtod_val_3",
        "dvtod_val_531",
        "dvtod_val_1005",
        "dvtod_val_1508",
        "dvtod_val_2180"
    };

    if (!verifyAuditSample(trainResults, expectedTrainAuditIds)) {
        return fail(
            24,
            "DVTOD train source-audit sample does not match Python."
        );
    }

    if (!verifyAuditSample(valResults, expectedValAuditIds)) {
        return fail(
            25,
            "DVTOD val source-audit sample does not match Python."
        );
    }

    std::cout
        << "[PASS] DVTOD real-data structural parity\n"
        << "  Native partitions: train, val\n"
        << "  Total pairs: 2179\n"
        << "  Train: 1606\n"
        << "  Val: 573\n"
        << "  Source-pair valid: 2179\n"
        << "  Same-geometry VALID: 0\n"
        << "  DIMENSION_MISMATCH: 2179\n"
        << "  Visible: 1920x1080\n"
        << "  Thermal: 640x512\n"
        << "  Ordered pair parity: exact\n"
        << "  Source-audit sample parity: exact\n"
        << "  Registration claim: NOT implied\n";

    return 0;
}
