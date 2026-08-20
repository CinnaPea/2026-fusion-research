//
// Created by hakgu on 8/20/2026.
//

#include "core/visualization/opencv_preview_renderer.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/imaging/image_dimensions.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/visualization/preview_renderer.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <type_traits>

namespace {

using qart::core::domain::datasets::
    DatasetPair;
using qart::core::domain::datasets::
    DatasetPartitionId;

using qart::core::domain::imaging::
    ImageDimensions;

using qart::core::domain::validation::
    PairValidationResult;
using qart::core::domain::validation::
    PairValidationStatus;

using qart::core::visualization::
    OpenCvPreviewRenderer;
using qart::core::visualization::
    PreviewRenderError;
using qart::core::visualization::
    PreviewRenderer;
using qart::core::visualization::
    PreviewRenderRequest;


class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
              std::filesystem::
                  temp_directory_path()
              / "qart_opencv_preview_renderer_contract"
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


PairValidationResult makeResult(
    const int width,
    const int height
)
{
    const ImageDimensions dimensions(
        width,
        height
    );

    return PairValidationResult(
        DatasetPair(
            "llvip_train_010001",
            "llvip",
            DatasetPartitionId("train"),
            "010001",
            "raw/llvip/visible/train/010001.png",
            "raw/llvip/infrared/train/010001.png"
        ),
        PairValidationStatus::Valid,
        true,
        true,
        dimensions,
        dimensions
    );
}


PreviewRenderRequest makeRequest(
    const std::filesystem::path& datasetRoot,
    const int width,
    const int height,
    const bool overwrite = false
)
{
    return PreviewRenderRequest(
        makeResult(
            width,
            height
        ),
        datasetRoot,
        datasetRoot
            / "derived"
            / "previews",
        overwrite
    );
}


void writeImage(
    const std::filesystem::path& path,
    const cv::Mat& image
)
{
    std::filesystem::create_directories(
        path.parent_path()
    );

    if (
        !cv::imwrite(
            path.string(),
            image
        )
    ) {
        throw std::runtime_error(
            "Could not create OpenCV test image."
        );
    }
}


bool matricesEqual(
    const cv::Mat& left,
    const cv::Mat& right
)
{
    if (
        left.size() != right.size()
        || left.type() != right.type()
    ) {
        return false;
    }

    cv::Mat difference;

    cv::compare(
        left,
        right,
        difference,
        cv::CMP_NE
    );

    return
        cv::countNonZero(
            difference.reshape(1)
        )
        == 0;
}

} // namespace


int main()
{
    static_assert(
        std::is_base_of_v<
            PreviewRenderer,
            OpenCvPreviewRenderer
        >
    );

    OpenCvPreviewRenderer renderer;

    // ------------------------------------------------------------
    // BGR body preserved and grayscale thermal replicated.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto request =
            makeRequest(
                temporary.path(),
                4,
                3
            );

        cv::Mat visible(
            3,
            4,
            CV_8UC3
        );

        for (int column = 0; column < 4; ++column) {
            visible.at<cv::Vec3b>(
                0,
                column
            ) = cv::Vec3b(
                10,
                20,
                30
            );

            visible.at<cv::Vec3b>(
                1,
                column
            ) = cv::Vec3b(
                40,
                50,
                60
            );

            visible.at<cv::Vec3b>(
                2,
                column
            ) = cv::Vec3b(
                70,
                80,
                90
            );
        }

        cv::Mat thermal(
            3,
            4,
            CV_8UC1
        );

        unsigned char value = 1;

        for (int row = 0; row < 3; ++row) {
            for (
                int column = 0;
                column < 4;
                ++column
            ) {
                thermal.at<unsigned char>(
                    row,
                    column
                ) = value++;
            }
        }

        writeImage(
            request.visibleSourcePath(),
            visible
        );

        writeImage(
            request.thermalSourcePath(),
            thermal
        );

        const auto result =
            renderer.render(
                request
            );

        const cv::Mat preview =
            cv::imread(
                result.outputPath().string(),
                cv::IMREAD_UNCHANGED
            );

        if (preview.empty()) {
            return 1;
        }

        if (
            preview.rows
            != 3 + result.headerHeight()
            || preview.cols != 8
            || preview.channels() != 3
        ) {
            return 2;
        }

        const cv::Rect visibleRegion(
            0,
            result.headerHeight(),
            4,
            3
        );

        const cv::Rect thermalRegion(
            4,
            result.headerHeight(),
            4,
            3
        );

        const cv::Mat visibleBody =
            preview(
                visibleRegion
            );

        const cv::Mat thermalBody =
            preview(
                thermalRegion
            );

        if (
            !matricesEqual(
                visibleBody,
                visible
            )
        ) {
            return 3;
        }

        cv::Mat expectedThermal;

        cv::cvtColor(
            thermal,
            expectedThermal,
            cv::COLOR_GRAY2BGR
        );

        if (
            !matricesEqual(
                thermalBody,
                expectedThermal
            )
        ) {
            return 4;
        }
    }

    // ------------------------------------------------------------
    // BGRA source converts to BGR only.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto request =
            makeRequest(
                temporary.path(),
                2,
                2
            );

        const cv::Mat visible =
            cv::Mat::zeros(
                2,
                2,
                CV_8UC3
            );

        cv::Mat thermalBgra(
            2,
            2,
            CV_8UC4
        );

        thermalBgra.at<cv::Vec4b>(
            0,
            0
        ) = cv::Vec4b(
            1, 2, 3, 100
        );

        thermalBgra.at<cv::Vec4b>(
            0,
            1
        ) = cv::Vec4b(
            4, 5, 6, 110
        );

        thermalBgra.at<cv::Vec4b>(
            1,
            0
        ) = cv::Vec4b(
            7, 8, 9, 120
        );

        thermalBgra.at<cv::Vec4b>(
            1,
            1
        ) = cv::Vec4b(
            10, 11, 12, 130
        );

        writeImage(
            request.visibleSourcePath(),
            visible
        );

        writeImage(
            request.thermalSourcePath(),
            thermalBgra
        );

        const auto result =
            renderer.render(
                request
            );

        const cv::Mat preview =
            cv::imread(
                result.outputPath().string(),
                cv::IMREAD_UNCHANGED
            );

        if (preview.empty()) {
            return 5;
        }

        const cv::Mat thermalBody =
            preview(
                cv::Rect(
                    2,
                    result.headerHeight(),
                    2,
                    2
                )
            );

        cv::Mat expectedThermal;

        cv::cvtColor(
            thermalBgra,
            expectedThermal,
            cv::COLOR_BGRA2BGR
        );

        if (
            !matricesEqual(
                thermalBody,
                expectedThermal
            )
        ) {
            return 6;
        }
    }

    // ------------------------------------------------------------
    // Missing source rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto request =
            makeRequest(
                temporary.path(),
                4,
                3
            );

        try {
            static_cast<void>(
                renderer.render(
                    request
                )
            );

            return 7;
        }
        catch (const PreviewRenderError&) {
        }
    }

    // ------------------------------------------------------------
    // Manifest-dimension drift rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto request =
            makeRequest(
                temporary.path(),
                4,
                3
            );

        writeImage(
            request.visibleSourcePath(),
            cv::Mat::zeros(
                3,
                5,
                CV_8UC3
            )
        );

        writeImage(
            request.thermalSourcePath(),
            cv::Mat::zeros(
                3,
                4,
                CV_8UC1
            )
        );

        try {
            static_cast<void>(
                renderer.render(
                    request
                )
            );

            return 8;
        }
        catch (const PreviewRenderError&) {
        }
    }

    // ------------------------------------------------------------
    // Non-8-bit source rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto request =
            makeRequest(
                temporary.path(),
                4,
                3
            );

        writeImage(
            request.visibleSourcePath(),
            cv::Mat::zeros(
                3,
                4,
                CV_16UC1
            )
        );

        try {
            static_cast<void>(
                renderer.render(
                    request
                )
            );

            return 9;
        }
        catch (const PreviewRenderError&) {
        }
    }

    // ------------------------------------------------------------
    // Refuse overwrite by default, replace explicitly.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto firstRequest =
            makeRequest(
                temporary.path(),
                4,
                3
            );

        writeImage(
            firstRequest.visibleSourcePath(),
            cv::Mat::zeros(
                3,
                4,
                CV_8UC3
            )
        );

        writeImage(
            firstRequest.thermalSourcePath(),
            cv::Mat::zeros(
                3,
                4,
                CV_8UC1
            )
        );

        const auto firstResult =
            renderer.render(
                firstRequest
            );

        try {
            static_cast<void>(
                renderer.render(
                    firstRequest
                )
            );

            return 10;
        }
        catch (const PreviewRenderError&) {
        }

        {
            std::ofstream obsolete(
                firstResult.outputPath(),
                std::ios::binary
            );

            obsolete
                << "obsolete preview";
        }

        const auto overwriteRequest =
            makeRequest(
                temporary.path(),
                4,
                3,
                true
            );

        const auto replaced =
            renderer.render(
                overwriteRequest
            );

        const cv::Mat decoded =
            cv::imread(
                replaced
                    .outputPath()
                    .string(),
                cv::IMREAD_UNCHANGED
            );

        if (decoded.empty()) {
            return 11;
        }
    }

    return 0;
}