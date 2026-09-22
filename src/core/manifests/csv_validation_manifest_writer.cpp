//
// Created by hakgu on 8/19/2026.
//

#include "core/manifests/csv_validation_manifest_writer.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/manifests/validation_manifest_row.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace qart::core::manifests {

namespace {

using domain::datasets::DatasetManifestError;

std::string lowercaseExtension(
    std::string extension
)
{
    for (char& character : extension) {
        if (
            character >= 'A'
            && character <= 'Z'
        ) {
            character =
                static_cast<char>(
                    character - 'A' + 'a'
                );
        }
    }

    return extension;
}

std::string escapeCsvField(
    const std::string& value
)
{
    const bool requiresQuotes =
        value.find_first_of(
            ",\"\r\n"
        ) != std::string::npos;

    if (!requiresQuotes) {
        return value;
    }

    std::string escaped;

    escaped.reserve(
        value.size() + 2
    );

    escaped.push_back('"');

    for (const char character : value) {
        if (character == '"') {
            escaped.push_back('"');
            escaped.push_back('"');
        }
        else {
            escaped.push_back(
                character
            );
        }
    }

    escaped.push_back('"');

    return escaped;
}

template <std::size_t Size>
std::string buildCsvRecord(
    const std::array<
        std::string,
        Size
    >& values
)
{
    std::string record;

    for (
        std::size_t index = 0;
        index < values.size();
        ++index
    ) {
        if (index != 0) {
            record.push_back(',');
        }

        record += escapeCsvField(
            values[index]
        );
    }

    record.push_back('\n');

    return record;
}

std::string buildHeader()
{
    std::array<
        std::string,
        kValidationManifestColumnCount
    > fields;

    for (
        std::size_t index = 0;
        index < fields.size();
        ++index
    ) {
        fields[index] =
            std::string(
                kValidationManifestColumns[index]
            );
    }

    return buildCsvRecord(
        fields
    );
}

class FileHandle final
{
public:
    explicit FileHandle(
        const std::filesystem::path& path
    )
    {
#ifdef _WIN32
        file_ = _wfopen(
            path.c_str(),
            L"wb"
        );
#else
        file_ = std::fopen(
            path.c_str(),
            "wb"
        );
#endif

        if (file_ == nullptr) {
            throw std::runtime_error(
                "Could not open temporary manifest file."
            );
        }
    }

    ~FileHandle()
    {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

    FileHandle(
        const FileHandle&
    ) = delete;

    FileHandle& operator=(
        const FileHandle&
    ) = delete;

    void write(
        const std::string& content
    ) const {
        const std::size_t written =
            std::fwrite(
                content.data(),
                sizeof(char),
                content.size(),
                file_
            );

        if (
            written
            != content.size()
        ) {
            throw std::runtime_error(
                "Could not write temporary manifest file."
            );
        }
    }

    void flushAndSync()
    {
        if (
            std::fflush(file_) != 0
        ) {
            throw std::runtime_error(
                "Could not flush temporary manifest file."
            );
        }

#ifdef _WIN32
        const int descriptor =
            _fileno(file_);

        if (
            descriptor < 0
            || _commit(descriptor) != 0
        ) {
            throw std::runtime_error(
                "Could not synchronize temporary manifest file."
            );
        }
#else
        const int descriptor =
            fileno(file_);

        if (
            descriptor < 0
            || fsync(descriptor) != 0
        ) {
            throw std::runtime_error(
                "Could not synchronize temporary manifest file."
            );
        }
#endif
    }

    void close()
    {
        if (file_ == nullptr) {
            return;
        }

        std::FILE* current =
            file_;

        file_ = nullptr;

        if (
            std::fclose(current) != 0
        ) {
            throw std::runtime_error(
                "Could not close temporary manifest file."
            );
        }
    }

private:
    std::FILE* file_ = nullptr;
};

std::filesystem::path makeTemporaryPath(
    const std::filesystem::path& outputPath
)
{
    const std::filesystem::path directory =
        outputPath.has_parent_path()
            ? outputPath.parent_path()
            : std::filesystem::current_path();

    std::random_device randomDevice;

    for (
        int attempt = 0;
        attempt < 32;
        ++attempt
    ) {
        const std::string nonce =
            std::to_string(
                randomDevice()
            )
            + "-"
            + std::to_string(
                randomDevice()
            );

        const std::filesystem::path candidate =
            directory
            / (
                "."
                + outputPath.filename().string()
                + "."
                + nonce
                + ".tmp"
            );

        if (
            !std::filesystem::exists(
                candidate
            )
        ) {
            return candidate;
        }
    }

    throw std::runtime_error(
        "Could not allocate a temporary manifest path."
    );
}

void replaceFile(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& outputPath,
    const bool overwrite
)
{
#ifdef _WIN32
    DWORD flags =
        MOVEFILE_WRITE_THROUGH;

    if (overwrite) {
        flags |=
            MOVEFILE_REPLACE_EXISTING;
    }

    if (
        !MoveFileExW(
            temporaryPath.c_str(),
            outputPath.c_str(),
            flags
        )
    ) {
        throw std::system_error(
            static_cast<int>(
                GetLastError()
            ),
            std::system_category(),
            "Could not replace validation manifest"
        );
    }
#else
    if (
        !overwrite
        && std::filesystem::exists(
            outputPath
        )
    ) {
        throw std::runtime_error(
            "Manifest output already exists."
        );
    }

    std::filesystem::rename(
        temporaryPath,
        outputPath
    );
#endif
}

void removeTemporaryFile(
    const std::filesystem::path& path
) noexcept
{
    std::error_code error;

    std::filesystem::remove(
        path,
        error
    );
}

} // namespace

std::filesystem::path
CsvValidationManifestWriter::write(
    const ValidationManifest& manifest,
    const std::filesystem::path& outputPath,
    const bool overwrite
) const
{
    if (
        manifest.schemaVersion()
        != kValidationManifestSchemaVersion
    ) {
        throw DatasetManifestError(
            "Validation manifest writer emits "
            "Schema 2.0 only."
        );
    }

    if (
        lowercaseExtension(
            outputPath.extension().string()
        )
        != ".csv"
    ) {
        throw DatasetManifestError(
            "Validation manifest output must use "
            "the .csv extension."
        );
    }

    try {
        if (
            std::filesystem::exists(
                outputPath
            )
            && std::filesystem::is_directory(
                outputPath
            )
        ) {
            throw DatasetManifestError(
                "Manifest output path is a directory: "
                + outputPath.string()
            );
        }

        if (
            std::filesystem::exists(
                outputPath
            )
            && !overwrite
        ) {
            throw DatasetManifestError(
                "Manifest output already exists and "
                "overwrite is false: "
                + outputPath.string()
            );
        }

        const std::filesystem::path directory =
            outputPath.has_parent_path()
                ? outputPath.parent_path()
                : std::filesystem::current_path();

        std::filesystem::create_directories(
            directory
        );

        const std::filesystem::path temporaryPath =
            makeTemporaryPath(
                outputPath
            );

        try {
            FileHandle temporaryFile(
                temporaryPath
            );

            temporaryFile.write(
                buildHeader()
            );

            for (
                const ValidationManifestRow& row
                : manifest.rows()
            ) {
                temporaryFile.write(
                    buildCsvRecord(
                        row.toCsvValues()
                    )
                );
            }

            temporaryFile.flushAndSync();
            temporaryFile.close();

            /*
             * Re-check no-overwrite immediately before replacement.
             * This keeps the writer conservative if the destination
             * appeared while the temporary file was being produced.
             */
            if (
                !overwrite
                && std::filesystem::exists(
                    outputPath
                )
            ) {
                throw DatasetManifestError(
                    "Manifest output already exists and "
                    "overwrite is false: "
                    + outputPath.string()
                );
            }

            replaceFile(
                temporaryPath,
                outputPath,
                overwrite
            );
        }
        catch (...) {
            removeTemporaryFile(
                temporaryPath
            );

            throw;
        }
    }
    catch (const DatasetManifestError&) {
        throw;
    }
    catch (
        const std::filesystem::filesystem_error&
    ) {
        throw DatasetManifestError(
            "Could not write validation manifest: "
            + outputPath.string()
        );
    }
    catch (const std::exception&) {
        throw DatasetManifestError(
            "Could not write validation manifest: "
            + outputPath.string()
        );
    }

    return outputPath;
}

} // namespace qart::core::manifests