//
// Created by hakgu on 8/20/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_PREVIEW_RENDERER_H
#define VISUAL_THERMAL_CONCEPT_PREVIEW_RENDERER_H

#pragma once

#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace qart::core::visualization {

class PreviewRenderError
    : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};


class PreviewRenderRequest final
{
public:
    PreviewRenderRequest(
        domain::validation::PairValidationResult result,
        std::filesystem::path datasetRoot,
        std::filesystem::path outputRoot,
        bool overwrite = false
    );

    [[nodiscard]]
    const domain::validation::PairValidationResult&
    result() const noexcept;

    [[nodiscard]]
    const std::filesystem::path&
    datasetRoot() const noexcept;

    [[nodiscard]]
    const std::filesystem::path&
    outputRoot() const noexcept;

    [[nodiscard]]
    bool overwrite() const noexcept;

    [[nodiscard]]
    std::filesystem::path
    visibleSourcePath() const;

    [[nodiscard]]
    std::filesystem::path
    thermalSourcePath() const;

    [[nodiscard]]
    std::filesystem::path
    outputPath() const;

private:
    static void validateOutputComponent(
        const std::string& value,
        const std::string& fieldName
    );

    void validate() const;

    domain::validation::PairValidationResult result_;
    std::filesystem::path datasetRoot_;
    std::filesystem::path outputRoot_;
    bool overwrite_;
};


class PreviewRenderResult final
{
public:
    PreviewRenderResult(
        std::string pairId,
        std::filesystem::path outputPath,
        domain::imaging::ImageDimensions sourceDimensions,
        domain::imaging::ImageDimensions previewDimensions,
        int headerHeight
    );

    [[nodiscard]]
    const std::string&
    pairId() const noexcept;

    [[nodiscard]]
    const std::filesystem::path&
    outputPath() const noexcept;

    [[nodiscard]]
    const domain::imaging::ImageDimensions&
    sourceDimensions() const noexcept;

    [[nodiscard]]
    const domain::imaging::ImageDimensions&
    previewDimensions() const noexcept;

    [[nodiscard]]
    int headerHeight() const noexcept;

private:
    void validate() const;

    std::string pairId_;
    std::filesystem::path outputPath_;
    domain::imaging::ImageDimensions sourceDimensions_;
    domain::imaging::ImageDimensions previewDimensions_;
    int headerHeight_;
};


class PreviewRenderer
{
public:
    virtual ~PreviewRenderer() = default;

    PreviewRenderer(
        const PreviewRenderer&
    ) = default;

    PreviewRenderer& operator=(
        const PreviewRenderer&
    ) = default;

    PreviewRenderer(
        PreviewRenderer&&
    ) noexcept = default;

    PreviewRenderer& operator=(
        PreviewRenderer&&
    ) noexcept = default;

    [[nodiscard]]
    virtual PreviewRenderResult render(
        const PreviewRenderRequest& request
    ) const = 0;

protected:
    PreviewRenderer() = default;
};

} // namespace qart::core::visualization

#endif //VISUAL_THERMAL_CONCEPT_PREVIEW_RENDERER_H