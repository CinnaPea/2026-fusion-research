//
// Created by hakgu on 8/20/2026.
//

#ifndef VISUAL_THERMAL_CONCEPT_OPENCV_PREVIEW_RENDERER_H
#define VISUAL_THERMAL_CONCEPT_OPENCV_PREVIEW_RENDERER_H

#pragma once

#include "core/visualization/preview_renderer.h"

namespace qart::core::visualization {

    class OpenCvPreviewRenderer final
        : public PreviewRenderer
    {
    public:
        [[nodiscard]]
        PreviewRenderResult render(
            const PreviewRenderRequest& request
        ) const override;
    };

} // namespace qart::core::visualization

#endif //VISUAL_THERMAL_CONCEPT_OPENCV_PREVIEW_RENDERER_H