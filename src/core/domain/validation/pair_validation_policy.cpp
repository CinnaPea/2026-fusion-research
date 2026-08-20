//
// Created by hakgu on 8/14/2026.
//

#include "core/domain/validation/pair_validation_policy.h"

#include <cctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace qart::core::domain::validation {

namespace {

std::string trim(
    const std::string& value
)
{
    std::size_t first = 0;

    while (
        first < value.size()
        && std::isspace(
            static_cast<unsigned char>(value[first])
        ) != 0
    ) {
        ++first;
    }

    std::size_t last = value.size();

    while (
        last > first
        && std::isspace(
            static_cast<unsigned char>(value[last - 1])
        ) != 0
    ) {
        --last;
    }

    return value.substr(
        first,
        last - first
    );
}

std::string toLower(
    std::string value
)
{
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(character)
            )
        );
    }

    return value;
}

} // namespace

PairValidationPolicy::PairValidationPolicy(
    std::vector<std::string> supportedVisibleExtensions,
    std::vector<std::string> supportedThermalExtensions,
    std::optional<imaging::ImageDimensions>
        expectedVisibleDimensions,
    std::optional<imaging::ImageDimensions>
        expectedThermalDimensions
)
    : supportedVisibleExtensions_(
          normalizeExtensions(
              supportedVisibleExtensions,
              "supported_visible_extensions"
          )
      ),
      supportedThermalExtensions_(
          normalizeExtensions(
              supportedThermalExtensions,
              "supported_thermal_extensions"
          )
      ),
      expectedVisibleDimensions_(
          std::move(expectedVisibleDimensions)
      ),
      expectedThermalDimensions_(
          std::move(expectedThermalDimensions)
      )
{
}

const std::vector<std::string>&
PairValidationPolicy::supportedVisibleExtensions() const noexcept
{
    return supportedVisibleExtensions_;
}

const std::vector<std::string>&
PairValidationPolicy::supportedThermalExtensions() const noexcept
{
    return supportedThermalExtensions_;
}

const std::optional<imaging::ImageDimensions>&
PairValidationPolicy::expectedVisibleDimensions() const noexcept
{
    return expectedVisibleDimensions_;
}

const std::optional<imaging::ImageDimensions>&
PairValidationPolicy::expectedThermalDimensions() const noexcept
{
    return expectedThermalDimensions_;
}

std::vector<std::string>
PairValidationPolicy::normalizeExtensions(
    const std::vector<std::string>& extensions,
    const char* fieldName
)
{
    if (extensions.empty()) {
        throw std::invalid_argument(
            std::string(fieldName)
            + " must contain at least one extension."
        );
    }

    std::set<std::string> normalizedExtensions;

    for (const std::string& extension : extensions) {
        const std::string normalizedExtension =
            toLower(
                trim(extension)
            );

        if (
            normalizedExtension.empty()
            || normalizedExtension == "."
            || normalizedExtension.front() != '.'
        ) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " contains an invalid extension: '"
                + extension
                + "'."
            );
        }

        if (
            normalizedExtension.find('/')
                != std::string::npos
            || normalizedExtension.find('\\')
                != std::string::npos
        ) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " extensions must not contain "
                  "directory separators."
            );
        }

        normalizedExtensions.insert(
            normalizedExtension
        );
    }

    return std::vector<std::string>(
        normalizedExtensions.begin(),
        normalizedExtensions.end()
    );
}

bool operator==(
    const PairValidationPolicy& left,
    const PairValidationPolicy& right
) noexcept
{
    return left.supportedVisibleExtensions_
            == right.supportedVisibleExtensions_
        && left.supportedThermalExtensions_
            == right.supportedThermalExtensions_
        && left.expectedVisibleDimensions_
            == right.expectedVisibleDimensions_
        && left.expectedThermalDimensions_
            == right.expectedThermalDimensions_;
}

bool operator!=(
    const PairValidationPolicy& left,
    const PairValidationPolicy& right
) noexcept
{
    return !(left == right);
}

} // namespace qart::core::domain::validation