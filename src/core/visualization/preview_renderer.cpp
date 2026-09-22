//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/preview_renderer.h"

#include "core/domain/validation/pair_validation_status.h"

#include <cctype>
#include <string>
#include <utility>

namespace qart::core::visualization {

namespace {

bool isBlank(
    const std::string& value
) noexcept
{
    if (value.empty()) {
        return true;
    }

    for (
        const unsigned char character
        : value
    ) {
        if (!std::isspace(character)) {
            return false;
        }
    }

    return true;
}

std::string lowercaseAscii(
    std::string value
)
{
    for (char& character : value) {
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

    return value;
}

} // namespace


PreviewRenderRequest::PreviewRenderRequest(
    domain::validation::PairValidationResult result,
    std::filesystem::path datasetRoot,
    std::filesystem::path outputRoot,
    const bool overwrite
)
    : result_(
          std::move(result)
      ),
      datasetRoot_(
          std::move(datasetRoot)
      ),
      outputRoot_(
          std::move(outputRoot)
      ),
      overwrite_(
          overwrite
      )
{
    validate();
}


void PreviewRenderRequest::validateOutputComponent(
    const std::string& value,
    const std::string& fieldName
)
{
    if (
        isBlank(value)
        || value == "."
        || value == ".."
        || value.find('/')
            != std::string::npos
        || value.find('\\')
            != std::string::npos
        || value.find(':')
            != std::string::npos
    ) {
        throw std::invalid_argument(
            fieldName
            + " must be one safe output-path component."
        );
    }
}


void PreviewRenderRequest::validate() const
{
    if (
        result_.status()
        != domain::validation::
            PairValidationStatus::Valid
    ) {
        throw std::invalid_argument(
            "Preview rendering requires a VALID pair result."
        );
    }

    if (
        !result_.visibleDimensions().has_value()
        || !result_.thermalDimensions().has_value()
    ) {
        throw std::invalid_argument(
            "Preview rendering requires both source dimensions."
        );
    }

    if (!datasetRoot_.is_absolute()) {
        throw std::invalid_argument(
            "Preview dataset_root must be absolute."
        );
    }

    if (!outputRoot_.is_absolute()) {
        throw std::invalid_argument(
            "Preview output_root must be absolute."
        );
    }

    validateOutputComponent(
        result_.pair().datasetId(),
        "dataset_id"
    );

    validateOutputComponent(
        result_.pair().pairId(),
        "pair_id"
    );

    /*
     * DatasetPartitionId already guarantees that the
     * partition value is a canonical single component.
     */
}


const domain::validation::PairValidationResult&
PreviewRenderRequest::result() const noexcept
{
    return result_;
}


const std::filesystem::path&
PreviewRenderRequest::datasetRoot() const noexcept
{
    return datasetRoot_;
}


const std::filesystem::path&
PreviewRenderRequest::outputRoot() const noexcept
{
    return outputRoot_;
}


bool
PreviewRenderRequest::overwrite() const noexcept
{
    return overwrite_;
}


std::filesystem::path
PreviewRenderRequest::visibleSourcePath() const
{
    return datasetRoot_
        / std::filesystem::path(
            result_
                .pair()
                .visibleRelativePath()
        );
}


std::filesystem::path
PreviewRenderRequest::thermalSourcePath() const
{
    return datasetRoot_
        / std::filesystem::path(
            result_
                .pair()
                .thermalRelativePath()
        );
}


std::filesystem::path
PreviewRenderRequest::outputPath() const
{
    return outputRoot_
        / result_.pair().datasetId()
        / result_
            .pair()
            .partitionId()
            .value()
        / (
            result_.pair().pairId()
            + ".png"
        );
}


PreviewRenderResult::PreviewRenderResult(
    std::string pairId,
    std::filesystem::path outputPath,
    domain::imaging::ImageDimensions sourceDimensions,
    domain::imaging::ImageDimensions previewDimensions,
    const int headerHeight
)
    : pairId_(
          std::move(pairId)
      ),
      outputPath_(
          std::move(outputPath)
      ),
      sourceDimensions_(
          std::move(sourceDimensions)
      ),
      previewDimensions_(
          std::move(previewDimensions)
      ),
      headerHeight_(
          headerHeight
      )
{
    validate();
}


void PreviewRenderResult::validate() const
{
    if (isBlank(pairId_)) {
        throw std::invalid_argument(
            "Preview result pair_id must not be empty."
        );
    }

    if (!outputPath_.is_absolute()) {
        throw std::invalid_argument(
            "Preview result output_path must be absolute."
        );
    }

    if (
        lowercaseAscii(
            outputPath_
                .extension()
                .string()
        )
        != ".png"
    ) {
        throw std::invalid_argument(
            "Preview result output_path must use .png."
        );
    }

    if (headerHeight_ <= 0) {
        throw std::invalid_argument(
            "Preview header_height must be positive."
        );
    }

    const int expectedPreviewWidth =
        sourceDimensions_.width()
        * 2;

    const int expectedPreviewHeight =
        sourceDimensions_.height()
        + headerHeight_;

    if (
        previewDimensions_.width()
        != expectedPreviewWidth
    ) {
        throw std::invalid_argument(
            "Preview width must equal twice the source width."
        );
    }

    if (
        previewDimensions_.height()
        != expectedPreviewHeight
    ) {
        throw std::invalid_argument(
            "Preview height must equal source height plus "
            "header_height."
        );
    }
}


const std::string&
PreviewRenderResult::pairId() const noexcept
{
    return pairId_;
}


const std::filesystem::path&
PreviewRenderResult::outputPath() const noexcept
{
    return outputPath_;
}


const domain::imaging::ImageDimensions&
PreviewRenderResult::sourceDimensions() const noexcept
{
    return sourceDimensions_;
}


const domain::imaging::ImageDimensions&
PreviewRenderResult::previewDimensions() const noexcept
{
    return previewDimensions_;
}


int
PreviewRenderResult::headerHeight() const noexcept
{
    return headerHeight_;
}

} // namespace qart::core::visualization