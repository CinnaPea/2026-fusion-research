//
// Created by hakgu on 8/19/2026.
//

#include "core/manifests/csv_validation_manifest_writer.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/manifests/csv_validation_manifest_reader.h"
#include "core/manifests/validation_manifest.h"
#include "core/manifests/validation_manifest_row.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using qart::core::domain::datasets::
    DatasetManifestError;

using qart::core::manifests::
    CsvValidationManifestReader;
using qart::core::manifests::
    CsvValidationManifestWriter;
using qart::core::manifests::
    ValidationManifest;
using qart::core::manifests::
    ValidationManifestRow;

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
              std::filesystem::temp_directory_path()
              / "qart_csv_manifest_writer_contract"
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

std::string readText(
    const std::filesystem::path& path
)
{
    std::ifstream stream(
        path,
        std::ios::binary
    );

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    return buffer.str();
}

bool hasTemporaryFiles(
    const std::filesystem::path& directory
)
{
    for (
        const auto& entry
        : std::filesystem::directory_iterator(
              directory
          )
    ) {
        const std::string name =
            entry.path()
                .filename()
                .string();

        if (
            !name.empty()
            && name.front() == '.'
            && name.size() >= 4
            && name.substr(
                   name.size() - 4
               )
               == ".tmp"
        ) {
            return true;
        }
    }

    return false;
}

ValidationManifest makeManifest()
{
    const std::string message =
        "Không đọc được ảnh \"nhiệt\", "
        "cần kiểm tra lại.\n"
        "Dòng chẩn đoán thứ hai.";

    std::vector<ValidationManifestRow>
        rows;

    rows.emplace_back(
        "llvip_train_010001",
        "llvip",
        "train",
        "010001",
        "raw/llvip/visible/train/010001.jpg",
        "raw/llvip/infrared/train/010001.jpg",
        "VALID",
        true,
        true,
        1280,
        1024,
        1280,
        1024,
        std::nullopt
    );

    rows.emplace_back(
        "llvip_train_010002",
        "llvip",
        "train",
        "010002",
        "raw/llvip/visible/train/010002.jpg",
        "raw/llvip/infrared/train/010002.jpg",
        "MISSING_VISIBLE",
        false,
        true,
        std::nullopt,
        std::nullopt,
        1280,
        1024,
        message
    );

    return ValidationManifest(
        "2.0",
        std::move(rows)
    );
}

} // namespace

int main()
{
    TemporaryDirectory temporaryDirectory;

    const auto root =
        temporaryDirectory.path();

    CsvValidationManifestWriter writer;
    CsvValidationManifestReader reader;

    const ValidationManifest manifest =
        makeManifest();

    // ------------------------------------------------------------
    // Write canonical Schema 2.0 file.
    // ------------------------------------------------------------

    const auto outputPath =
        root / "validation.csv";

    const auto returnedPath =
        writer.write(
            manifest,
            outputPath
        );

    if (returnedPath != outputPath) {
        return 1;
    }

    if (
        !std::filesystem::is_regular_file(
            outputPath
        )
    ) {
        return 2;
    }

    const std::string raw =
        readText(
            outputPath
        );

    const std::string expectedHeader =
        "schema_version,pair_id,dataset_id,"
        "partition_id,source_stem,"
        "visible_relative_path,"
        "thermal_relative_path,status,"
        "visible_decoded,thermal_decoded,"
        "visible_width,visible_height,"
        "thermal_width,thermal_height,message\n";

    if (
        raw.rfind(
            expectedHeader,
            0
        ) != 0
    ) {
        return 3;
    }

    if (
        raw.find(",split,")
        != std::string::npos
    ) {
        return 4;
    }

    // ------------------------------------------------------------
    // Writer -> reader must reproduce typed manifest exactly.
    // ------------------------------------------------------------

    const ValidationManifest loaded =
        reader.read(
            outputPath
        );

    if (loaded != manifest) {
        return 5;
    }

    // ------------------------------------------------------------
    // UTF-8 / comma / quotes / newline must survive CSV escaping.
    // ------------------------------------------------------------

    if (
        loaded.rows()
            .at(1)
            .message()
        != manifest.rows()
            .at(1)
            .message()
    ) {
        return 6;
    }

    if (
        raw.find(
            "\"Không đọc được ảnh \"\"nhiệt\"\", "
        )
        == std::string::npos
    ) {
        return 7;
    }

    if (hasTemporaryFiles(root)) {
        return 8;
    }

    // ------------------------------------------------------------
    // No overwrite by default.
    // ------------------------------------------------------------

    const std::string originalContents =
        readText(
            outputPath
        );

    try {
        static_cast<void>(
            writer.write(
                manifest,
                outputPath
            )
        );

        return 9;
    }
    catch (const DatasetManifestError&) {
    }

    if (
        readText(outputPath)
        != originalContents
    ) {
        return 10;
    }

    // ------------------------------------------------------------
    // Explicit overwrite replaces destination.
    // ------------------------------------------------------------

    {
        std::ofstream obsolete(
            outputPath,
            std::ios::binary
        );

        obsolete
            << "obsolete contents\n";
    }

    static_cast<void>(
        writer.write(
            manifest,
            outputPath,
            true
        )
    );

    if (
        readText(outputPath)
        == "obsolete contents\n"
    ) {
        return 11;
    }

    if (
        reader.read(outputPath)
        != manifest
    ) {
        return 12;
    }

    if (hasTemporaryFiles(root)) {
        return 13;
    }

    // ------------------------------------------------------------
    // Empty manifest emits header only.
    // ------------------------------------------------------------

    const ValidationManifest emptyManifest(
        "2.0",
        {}
    );

    const auto emptyPath =
        root / "empty.csv";

    static_cast<void>(
        writer.write(
            emptyManifest,
            emptyPath
        )
    );

    if (
        readText(emptyPath)
        != expectedHeader
    ) {
        return 14;
    }

    // ------------------------------------------------------------
    // Missing parent directories are created.
    // ------------------------------------------------------------

    const auto nestedPath =
        root
        / "outputs"
        / "manifests"
        / "validation.csv";

    if (
        std::filesystem::exists(
            nestedPath.parent_path()
        )
    ) {
        return 15;
    }

    static_cast<void>(
        writer.write(
            emptyManifest,
            nestedPath
        )
    );

    if (
        !std::filesystem::is_regular_file(
            nestedPath
        )
    ) {
        return 16;
    }

    // ------------------------------------------------------------
    // Non-CSV output rejected.
    // ------------------------------------------------------------

    try {
        static_cast<void>(
            writer.write(
                emptyManifest,
                root / "validation.json"
            )
        );

        return 17;
    }
    catch (const DatasetManifestError&) {
    }

    // ------------------------------------------------------------
    // Directory masquerading as .csv rejected.
    // ------------------------------------------------------------

    const auto directoryPath =
        root / "directory.csv";

    std::filesystem::create_directory(
        directoryPath
    );

    try {
        static_cast<void>(
            writer.write(
                emptyManifest,
                directoryPath,
                true
            )
        );

        return 18;
    }
    catch (const DatasetManifestError&) {
    }

    return 0;
}