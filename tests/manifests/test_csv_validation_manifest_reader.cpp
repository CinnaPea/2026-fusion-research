//
// Created by hakgu on 8/18/2026.
//

#include "core/manifests/csv_validation_manifest_reader.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/validation/pair_validation_status.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::domain::datasets::
    DatasetManifestError;

using qart::core::domain::validation::
    PairValidationStatus;

using qart::core::manifests::
    CsvValidationManifestReader;

constexpr const char* canonicalHeader =
    "schema_version,pair_id,dataset_id,partition_id,"
    "source_stem,visible_relative_path,"
    "thermal_relative_path,status,"
    "visible_decoded,thermal_decoded,"
    "visible_width,visible_height,"
    "thermal_width,thermal_height,message\n";

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
              std::filesystem::temp_directory_path()
              / "qart_csv_manifest_reader_contract"
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

void writeText(
    const std::filesystem::path& path,
    const std::string& content
)
{
    std::ofstream stream(
        path,
        std::ios::binary
    );

    stream.write(
        content.data(),
        static_cast<std::streamsize>(
            content.size()
        )
    );
}

bool expectManifestError(
    const CsvValidationManifestReader& reader,
    const std::filesystem::path& path,
    const std::string& expectedText
)
{
    try {
        static_cast<void>(
            reader.read(path)
        );
    }
    catch (const DatasetManifestError& error) {
        return std::string(
            error.what()
        ).find(
            expectedText
        ) != std::string::npos;
    }

    return false;
}

} // namespace

int main()
{
    TemporaryDirectory temporaryDirectory;

    const auto root =
        temporaryDirectory.path();

    CsvValidationManifestReader reader;

    // ------------------------------------------------------------
    // Valid Schema 2.0 + multiline quoted UTF-8 message.
    // ------------------------------------------------------------

    const auto validPath =
        root / "valid.csv";

    const std::string message =
        "Không đọc được ảnh \"màu\".\n"
        "Dòng chẩn đoán thứ hai.";

    writeText(
        validPath,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "raw/llvip/visible/train/010001.jpg,"
          "raw/llvip/infrared/train/010001.jpg,"
          "VALID,true,true,1280,1024,1280,1024,\n"

        + "2.0,llvip_train_010002,llvip,train,010002,"
          "raw/llvip/visible/train/010002.jpg,"
          "raw/llvip/infrared/train/010002.jpg,"
          "MISSING_VISIBLE,false,true,,,1280,1024,"
          "\"Không đọc được ảnh \"\"màu\"\".\n"
          "Dòng chẩn đoán thứ hai.\"\n"
    );

    const auto manifest =
        reader.read(
            validPath
        );

    if (
        manifest.schemaVersion()
        != "2.0"
    ) {
        return 1;
    }

    if (manifest.rows().size() != 2) {
        return 2;
    }

    if (
        manifest.rows().at(0).pairId()
        != "llvip_train_010001"
    ) {
        return 3;
    }

    if (
        manifest.rows().at(1).message()
        != std::optional<std::string>(
            message
        )
    ) {
        return 4;
    }

    const auto results =
        manifest.toResults();

    if (results.size() != 2) {
        return 5;
    }

    if (
        results.at(0).status()
        != PairValidationStatus::Valid
    ) {
        return 6;
    }

    if (
        results.at(1).status()
        != PairValidationStatus::MissingVisible
    ) {
        return 7;
    }

    // ------------------------------------------------------------
    // Header-only manifest is valid.
    // ------------------------------------------------------------

    const auto emptyPath =
        root / "empty.csv";

    writeText(
        emptyPath,
        canonicalHeader
    );

    const auto emptyManifest =
        reader.read(
            emptyPath
        );

    if (!emptyManifest.rows().empty()) {
        return 8;
    }

    // ------------------------------------------------------------
    // Non-CSV extension.
    // ------------------------------------------------------------

    const auto wrongExtension =
        root / "manifest.json";

    writeText(
        wrongExtension,
        canonicalHeader
    );

    if (
        !expectManifestError(
            reader,
            wrongExtension,
            ".csv extension"
        )
    ) {
        return 9;
    }

    // ------------------------------------------------------------
    // Missing file.
    // ------------------------------------------------------------

    if (
        !expectManifestError(
            reader,
            root / "missing.csv",
            "does not exist"
        )
    ) {
        return 10;
    }

    // ------------------------------------------------------------
    // Incorrect header order.
    // ------------------------------------------------------------

    const auto badHeader =
        root / "bad-header.csv";

    writeText(
        badHeader,
        "pair_id,schema_version,dataset_id,partition_id,"
        "source_stem,visible_relative_path,"
        "thermal_relative_path,status,"
        "visible_decoded,thermal_decoded,"
        "visible_width,visible_height,"
        "thermal_width,thermal_height,message\n"
    );

    if (
        !expectManifestError(
            reader,
            badHeader,
            "header"
        )
    ) {
        return 11;
    }

    // ------------------------------------------------------------
    // Wrong row column count.
    // ------------------------------------------------------------

    const auto shortRow =
        root / "short-row.csv";

    writeText(
        shortRow,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "visible.jpg,thermal.jpg,VALID,true,true,"
          "1280,1024,1280,1024\n"
    );

    if (
        !expectManifestError(
            reader,
            shortRow,
            "Expected 15 columns but found 14"
        )
    ) {
        return 12;
    }

    // ------------------------------------------------------------
    // Schema version disagrees with Schema 2.0 header.
    // ------------------------------------------------------------

    const auto wrongSchema =
        root / "wrong-schema.csv";

    writeText(
        wrongSchema,
        std::string(canonicalHeader)
        + "3.0,llvip_train_010001,llvip,train,010001,"
          "visible.jpg,thermal.jpg,VALID,true,true,"
          "1280,1024,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            wrongSchema,
            "disagrees with its Schema 2.0 header"
        )
    ) {
        return 13;
    }

    // ------------------------------------------------------------
    // Boolean must be canonical lowercase.
    // ------------------------------------------------------------

    const auto badBoolean =
        root / "bad-boolean.csv";

    writeText(
        badBoolean,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "visible.jpg,thermal.jpg,VALID,yes,true,"
          "1280,1024,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            badBoolean,
            "exactly 'true' or 'false'"
        )
    ) {
        return 14;
    }

    // ------------------------------------------------------------
    // Native partition must be canonical.
    // ------------------------------------------------------------

    const auto badPartition =
        root / "bad-partition.csv";

    writeText(
        badPartition,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,Train,010001,"
          "visible.jpg,thermal.jpg,VALID,true,true,"
          "1280,1024,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            badPartition,
            "row 2"
        )
    ) {
        return 15;
    }

    // ------------------------------------------------------------
    // Unknown validation status.
    // ------------------------------------------------------------

    const auto badStatus =
        root / "bad-status.csv";

    writeText(
        badStatus,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "visible.jpg,thermal.jpg,UNKNOWN_STATUS,"
          "true,true,1280,1024,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            badStatus,
            "Unknown validation status"
        )
    ) {
        return 16;
    }

    // ------------------------------------------------------------
    // Partial dimensions.
    // ------------------------------------------------------------

    const auto partialDimensions =
        root / "partial-dimensions.csv";

    writeText(
        partialDimensions,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "visible.jpg,thermal.jpg,VALID,true,true,"
          "1280,,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            partialDimensions,
            "width and height must either both exist"
        )
    ) {
        return 17;
    }

    // ------------------------------------------------------------
    // Duplicate pair IDs retain logical row information.
    // ------------------------------------------------------------

    const auto duplicates =
        root / "duplicates.csv";

    const std::string duplicateRow =
        "2.0,llvip_train_010001,llvip,train,010001,"
        "visible.jpg,thermal.jpg,VALID,true,true,"
        "1280,1024,1280,1024,\n";

    writeText(
        duplicates,
        std::string(canonicalHeader)
        + duplicateRow
        + duplicateRow
    );

    if (
        !expectManifestError(
            reader,
            duplicates,
            "row 3 duplicates pair_id"
        )
    ) {
        return 18;
    }

    // ------------------------------------------------------------
    // Decoded/dimension inconsistency.
    // ------------------------------------------------------------

    const auto inconsistent =
        root / "inconsistent.csv";

    writeText(
        inconsistent,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "visible.jpg,thermal.jpg,VALID,false,true,"
          "1280,1024,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            inconsistent,
            "row 2"
        )
    ) {
        return 19;
    }

    // ------------------------------------------------------------
    // Portable relative paths cannot escape shared root.
    // ------------------------------------------------------------

    const auto badPath =
        root / "bad-path.csv";

    writeText(
        badPath,
        std::string(canonicalHeader)
        + "2.0,llvip_train_010001,llvip,train,010001,"
          "../outside.jpg,thermal.jpg,VALID,true,true,"
          "1280,1024,1280,1024,\n"
    );

    if (
        !expectManifestError(
            reader,
            badPath,
            "row 2"
        )
    ) {
        return 20;
    }

    return 0;
}