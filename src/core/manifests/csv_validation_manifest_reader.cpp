//
// Created by hakgu on 8/18/2026.
//
#include "core/manifests/csv_validation_manifest_reader.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/manifests/validation_manifest_row.h"

#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qart::core::manifests {

namespace {

using domain::datasets::DatasetManifestError;

class CsvParseError final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

bool isAsciiWhitespace(
    const char character
) noexcept
{
    switch (character) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\f':
        case '\v':
            return true;

        default:
            return false;
    }
}

bool isBlank(
    const std::string& value
) noexcept
{
    if (value.empty()) {
        return true;
    }

    for (const char character : value) {
        if (!isAsciiWhitespace(character)) {
            return false;
        }
    }

    return true;
}

std::string parseRequiredText(
    const std::string& value,
    const std::string& fieldName
)
{
    if (isBlank(value)) {
        throw std::invalid_argument(
            "Manifest field '"
            + fieldName
            + "' must not be empty."
        );
    }

    if (
        isAsciiWhitespace(value.front())
        || isAsciiWhitespace(value.back())
    ) {
        throw std::invalid_argument(
            "Manifest field '"
            + fieldName
            + "' must not contain leading or trailing whitespace."
        );
    }

    return value;
}

bool parseBoolean(
    const std::string& value,
    const std::string& fieldName
)
{
    if (value == "true") {
        return true;
    }

    if (value == "false") {
        return false;
    }

    throw std::invalid_argument(
        "Manifest field '"
        + fieldName
        + "' must be exactly 'true' or 'false'."
    );
}

std::optional<int>
parseOptionalPositiveInteger(
    const std::string& value,
    const std::string& fieldName
)
{
    if (value.empty()) {
        return std::nullopt;
    }

    int parsedValue = 0;

    const char* begin =
        value.data();

    const char* end =
        value.data() + value.size();

    const auto result =
        std::from_chars(
            begin,
            end,
            parsedValue
        );

    if (
        result.ec != std::errc{}
        || result.ptr != end
    ) {
        throw std::invalid_argument(
            "Manifest field '"
            + fieldName
            + "' must be a positive integer or empty."
        );
    }

    if (parsedValue <= 0) {
        throw std::invalid_argument(
            "Manifest field '"
            + fieldName
            + "' must be positive."
        );
    }

    return parsedValue;
}

/*
 * Read one logical CSV record.
 *
 * Quoted fields may contain:
 * - commas
 * - doubled quotes
 * - physical newlines
 *
 * Returns false only when EOF is reached before a new record begins.
 */
bool readCsvRecord(
    std::istream& input,
    std::vector<std::string>& fields
)
{
    fields.clear();

    std::string field;

    bool recordStarted = false;
    bool inQuotes = false;
    bool afterClosingQuote = false;
    bool atFieldStart = true;

    while (true) {
        const int nextValue =
            input.get();

        if (nextValue == EOF) {
            if (input.bad()) {
                throw CsvParseError(
                    "CSV stream encountered an I/O error."
                );
            }

            if (inQuotes) {
                throw CsvParseError(
                    "CSV contains an unterminated quoted field."
                );
            }

            if (!recordStarted) {
                return false;
            }

            fields.push_back(
                std::move(field)
            );

            return true;
        }

        recordStarted = true;

        const char character =
            static_cast<char>(nextValue);

        if (inQuotes) {
            if (character == '"') {
                if (input.peek() == '"') {
                    input.get();
                    field.push_back('"');
                }
                else {
                    inQuotes = false;
                    afterClosingQuote = true;
                }
            }
            else {
                field.push_back(
                    character
                );
            }

            continue;
        }

        if (afterClosingQuote) {
            if (character == ',') {
                fields.push_back(
                    std::move(field)
                );

                field.clear();

                afterClosingQuote = false;
                atFieldStart = true;

                continue;
            }

            if (character == '\n') {
                fields.push_back(
                    std::move(field)
                );

                return true;
            }

            if (character == '\r') {
                if (input.peek() == '\n') {
                    input.get();
                }

                fields.push_back(
                    std::move(field)
                );

                return true;
            }

            throw CsvParseError(
                "CSV contains characters after a closing quote."
            );
        }

        if (character == ',') {
            fields.push_back(
                std::move(field)
            );

            field.clear();
            atFieldStart = true;

            continue;
        }

        if (character == '\n') {
            fields.push_back(
                std::move(field)
            );

            return true;
        }

        if (character == '\r') {
            if (input.peek() == '\n') {
                input.get();
            }

            fields.push_back(
                std::move(field)
            );

            return true;
        }

        if (character == '"') {
            if (!atFieldStart) {
                throw CsvParseError(
                    "CSV contains an unexpected quote "
                    "inside an unquoted field."
                );
            }

            inQuotes = true;
            atFieldStart = false;

            continue;
        }

        field.push_back(
            character
        );

        atFieldStart = false;
    }
}

bool hasCanonicalHeader(
    const std::vector<std::string>& header
)
{
    if (
        header.size()
        != kValidationManifestColumns.size()
    ) {
        return false;
    }

    for (
        std::size_t index = 0;
        index < header.size();
        ++index
    ) {
        if (
            header[index]
            != kValidationManifestColumns[index]
        ) {
            return false;
        }
    }

    return true;
}

ValidationManifestRow parseRow(
    const std::vector<std::string>& values
)
{
    if (
        values.size()
        != kValidationManifestColumnCount
    ) {
        throw std::invalid_argument(
            "Expected "
            + std::to_string(
                kValidationManifestColumnCount
            )
            + " columns but found "
            + std::to_string(
                values.size()
            )
            + "."
        );
    }

    const std::string schemaVersion =
        parseRequiredText(
            values[0],
            "schema_version"
        );

    if (
        schemaVersion
        != kValidationManifestSchemaVersion
    ) {
        throw std::invalid_argument(
            "Validation manifest row schema_version '"
            + schemaVersion
            + "' disagrees with its Schema 2.0 header."
        );
    }

    ValidationManifestRow row(
        parseRequiredText(
            values[1],
            "pair_id"
        ),
        parseRequiredText(
            values[2],
            "dataset_id"
        ),
        parseRequiredText(
            values[3],
            "partition_id"
        ),
        parseRequiredText(
            values[4],
            "source_stem"
        ),
        parseRequiredText(
            values[5],
            "visible_relative_path"
        ),
        parseRequiredText(
            values[6],
            "thermal_relative_path"
        ),
        parseRequiredText(
            values[7],
            "status"
        ),
        parseBoolean(
            values[8],
            "visible_decoded"
        ),
        parseBoolean(
            values[9],
            "thermal_decoded"
        ),
        parseOptionalPositiveInteger(
            values[10],
            "visible_width"
        ),
        parseOptionalPositiveInteger(
            values[11],
            "visible_height"
        ),
        parseOptionalPositiveInteger(
            values[12],
            "thermal_width"
        ),
        parseOptionalPositiveInteger(
            values[13],
            "thermal_height"
        ),
        values[14].empty()
            ? std::nullopt
            : std::optional<std::string>(
                  values[14]
              )
    );

    /*
     * Force reconstruction now.
     *
     * This validates:
     * - status identity
     * - DatasetPair/path contract
     * - decoded/dimension consistency
     * - PairValidationResult invariants
     */
    static_cast<void>(
        row.toResult()
    );

    return row;
}

std::string lowercaseExtension(
    std::string extension
)
{
    for (char& character : extension) {
        character =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        character
                    )
                )
            );
    }

    return extension;
}

} // namespace

ValidationManifest
CsvValidationManifestReader::read(
    const std::filesystem::path& inputPath
) const
{
    if (
        lowercaseExtension(
            inputPath.extension().string()
        )
        != ".csv"
    ) {
        throw DatasetManifestError(
            "Validation manifest input must use "
            "the .csv extension."
        );
    }

    if (
        !std::filesystem::exists(
            inputPath
        )
    ) {
        throw DatasetManifestError(
            "Validation manifest input does not exist: "
            + inputPath.string()
        );
    }

    if (
        std::filesystem::is_directory(
            inputPath
        )
    ) {
        throw DatasetManifestError(
            "Validation manifest input path is a directory: "
            + inputPath.string()
        );
    }

    if (
        !std::filesystem::is_regular_file(
            inputPath
        )
    ) {
        throw DatasetManifestError(
            "Validation manifest input is not a regular file: "
            + inputPath.string()
        );
    }

    std::ifstream manifestFile(
        inputPath,
        std::ios::binary
    );

    if (!manifestFile) {
        throw DatasetManifestError(
            "Could not read validation manifest: "
            + inputPath.string()
        );
    }

    std::vector<std::string> header;

    try {
        if (
            !readCsvRecord(
                manifestFile,
                header
            )
        ) {
            throw DatasetManifestError(
                "Validation manifest is empty and has no header."
            );
        }

        if (!hasCanonicalHeader(header)) {
            throw DatasetManifestError(
                "Validation manifest header does not match "
                "canonical Schema 2.0."
            );
        }

        std::vector<ValidationManifestRow>
            rows;

        std::unordered_map<
            std::string,
            std::size_t
        > firstRowByPairId;

        std::vector<std::string>
            rawValues;

        std::size_t logicalRowNumber = 2;

        while (
            readCsvRecord(
                manifestFile,
                rawValues
            )
        ) {
            ValidationManifestRow row =
                [&]() {
                    try {
                        return parseRow(
                            rawValues
                        );
                    }
                    catch (
                        const std::invalid_argument& error
                    ) {
                        throw DatasetManifestError(
                            "Validation manifest row "
                            + std::to_string(
                                logicalRowNumber
                            )
                            + " is invalid: "
                            + error.what()
                        );
                    }
                }();

            const auto existing =
                firstRowByPairId.find(
                    row.pairId()
                );

            if (
                existing
                != firstRowByPairId.end()
            ) {
                throw DatasetManifestError(
                    "Validation manifest row "
                    + std::to_string(
                        logicalRowNumber
                    )
                    + " duplicates pair_id '"
                    + row.pairId()
                    + "', first seen at row "
                    + std::to_string(
                        existing->second
                    )
                    + "."
                );
            }

            firstRowByPairId.emplace(
                row.pairId(),
                logicalRowNumber
            );

            rows.push_back(
                std::move(row)
            );

            ++logicalRowNumber;
        }

        return ValidationManifest(
            std::string(
                kValidationManifestSchemaVersion
            ),
            std::move(rows)
        );
    }
    catch (const DatasetManifestError&) {
        throw;
    }
    catch (const CsvParseError& error) {
        throw DatasetManifestError(
            "Could not parse validation manifest: "
            + inputPath.string()
            + ". "
            + error.what()
        );
    }
    catch (const std::filesystem::filesystem_error& error) {
        throw DatasetManifestError(
            "Could not read validation manifest: "
            + inputPath.string()
            + ". "
            + error.what()
        );
    }
}

} // namespace qart::core::manifests