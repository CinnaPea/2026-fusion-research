//
// Created by hakgu on 8/18/2026.
//

#include "core/manifests/validation_manifest.h"

#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace qart::core::manifests {

namespace {

bool isBlank(
    const std::string& value
) noexcept
{
    return value.find_first_not_of(
        " \t\n\r\f\v"
    ) == std::string::npos;
}

} // namespace

ValidationManifest::ValidationManifest(
    std::string schemaVersion,
    std::vector<ValidationManifestRow> rows
)
    : schemaVersion_(
          std::move(schemaVersion)
      ),
      rows_(
          std::move(rows)
      )
{
    validate();
}

void ValidationManifest::validate() const
{
    if (isBlank(schemaVersion_)) {
        throw std::invalid_argument(
            "Manifest schema_version must not be empty."
        );
    }

    if (
        schemaVersion_
        != kValidationManifestSchemaVersion
    ) {
        throw std::invalid_argument(
            "Unsupported validation manifest "
            "schema_version: '"
            + schemaVersion_
            + "'. C++ supports canonical Schema 2.0."
        );
    }

    std::set<std::string>
        observedPairIds;

    for (
        const ValidationManifestRow& row
        : rows_
    ) {
        const auto insertion =
            observedPairIds.insert(
                row.pairId()
            );

        if (!insertion.second) {
            throw std::invalid_argument(
                "Validation manifest rows must "
                "contain unique pair IDs."
            );
        }
    }
}

ValidationManifest
ValidationManifest::fromReport(
    const domain::validation::BatchValidationReport& report
)
{
    std::vector<ValidationManifestRow>
        rows;

    rows.reserve(
        report.results().size()
    );

    for (
        const domain::validation::PairValidationResult& result
        : report.results()
    ) {
        rows.push_back(
            ValidationManifestRow::fromResult(
                result
            )
        );
    }

    return ValidationManifest(
        std::string(
            kValidationManifestSchemaVersion
        ),
        std::move(rows)
    );
}

std::vector<
    domain::validation::PairValidationResult
> ValidationManifest::toResults() const
{
    std::vector<
        domain::validation::PairValidationResult
    > results;

    results.reserve(
        rows_.size()
    );

    for (
        const ValidationManifestRow& row
        : rows_
    ) {
        results.push_back(
            row.toResult()
        );
    }

    return results;
}

const std::string&
ValidationManifest::schemaVersion() const noexcept
{
    return schemaVersion_;
}

const std::vector<ValidationManifestRow>&
ValidationManifest::rows() const noexcept
{
    return rows_;
}

bool operator==(
    const ValidationManifest& left,
    const ValidationManifest& right
) noexcept
{
    return left.schemaVersion_
            == right.schemaVersion_
        && left.rows_
            == right.rows_;
}

bool operator!=(
    const ValidationManifest& left,
    const ValidationManifest& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::manifests