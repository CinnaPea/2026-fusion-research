//
// Created by hakgu on 8/20/2026.
//

#include "core/datasets/roadscene_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::datasets::RoadSceneAdapter;

using qart::core::domain::datasets::
    DatasetLayoutError;
using qart::core::domain::datasets::
    DatasetPairingError;
using qart::core::domain::datasets::
    DatasetPartitionId;

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(
              std::filesystem::temp_directory_path()
              / "qart_roadscene_adapter_contract"
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

void createCanonicalLayout(
    const std::filesystem::path& root
)
{
    for (
        const std::string directoryName
        : {
            std::string("crop_LR_visible"),
            std::string("cropinfrared")
        }
    ) {
        std::filesystem::create_directories(
            root
            / "raw"
            / "roadscene"
            / directoryName
        );
    }
}

void createCompleteNativeLayout(
    const std::filesystem::path& root
)
{
    createCanonicalLayout(
        root
    );

    for (
        const std::string directoryName
        : {
            std::string("crop_HR_visible"),
            std::string("infrared")
        }
    ) {
        std::filesystem::create_directories(
            root
            / "raw"
            / "roadscene"
            / directoryName
        );
    }
}

void createFile(
    const std::filesystem::path& root,
    const std::string& directoryName,
    const std::string& filename
)
{
    std::ofstream stream(
        root
        / "raw"
        / "roadscene"
        / directoryName
        / filename,
        std::ios::binary
    );
}

} // namespace

int main()
{
    RoadSceneAdapter adapter;

    // ------------------------------------------------------------
    // Native identity: exactly one "all" partition.
    // ------------------------------------------------------------

    if (
        adapter.datasetId()
        != "roadscene"
    ) {
        return 1;
    }

    const auto partitions =
        adapter.supportedPartitions();

    if (partitions.size() != 1) {
        return 2;
    }

    if (
        partitions.at(0)
        != DatasetPartitionId("all")
    ) {
        return 3;
    }

    if (
        !adapter.supportsPartition("all")
    ) {
        return 4;
    }

    if (
        adapter.supportsPartition("test")
    ) {
        return 5;
    }

    // ------------------------------------------------------------
    // Only canonical directories are required.
    // Auxiliary directories are not layout requirements.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCanonicalLayout(
            temporary.path()
        );

        adapter.validateLayout(
            temporary.path()
        );
    }

    // ------------------------------------------------------------
    // Missing canonical thermal directory rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        std::filesystem::create_directories(
            temporary.path()
            / "raw"
            / "roadscene"
            / "crop_LR_visible"
        );

        try {
            adapter.validateLayout(
                temporary.path()
            );

            return 6;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Standard and video stems sorted deterministically.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteNativeLayout(
            root
        );

        for (
            const std::string filename
            : {
                std::string(
                    "FLIR_video_00003.jpg"
                ),
                std::string(
                    "FLIR_00018.jpg"
                ),
                std::string(
                    "FLIR_00006.jpg"
                )
            }
        ) {
            createFile(
                root,
                "crop_LR_visible",
                filename
            );

            createFile(
                root,
                "cropinfrared",
                filename
            );
        }

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (pairs.size() != 3) {
            return 7;
        }

        if (
            pairs.at(0).pairId()
            != "roadscene_all_FLIR_00006"
        ) {
            return 8;
        }

        if (
            pairs.at(1).pairId()
            != "roadscene_all_FLIR_00018"
        ) {
            return 9;
        }

        if (
            pairs.at(2).pairId()
            != "roadscene_all_FLIR_video_00003"
        ) {
            return 10;
        }

        for (const auto& pair : pairs) {
            if (
                pair.partitionId()
                != DatasetPartitionId("all")
            ) {
                return 11;
            }
        }
    }

    // ------------------------------------------------------------
    // Visible-only candidate remains discoverable for validation.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCanonicalLayout(
            root
        );

        createFile(
            root,
            "crop_LR_visible",
            "FLIR_00006.jpg"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (pairs.size() != 1) {
            return 12;
        }

        if (
            pairs.at(0).visibleRelativePath()
            !=
            "raw/roadscene/crop_LR_visible/FLIR_00006.jpg"
        ) {
            return 13;
        }

        if (
            pairs.at(0).thermalRelativePath()
            !=
            "raw/roadscene/cropinfrared/FLIR_00006.jpg"
        ) {
            return 14;
        }
    }

    // ------------------------------------------------------------
    // Auxiliary directories and unsupported files are ignored.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteNativeLayout(
            root
        );

        createFile(
            root,
            "crop_HR_visible",
            "FLIR_00006.jpg"
        );

        createFile(
            root,
            "infrared",
            "FLIR_00006.png"
        );

        createFile(
            root,
            "crop_LR_visible",
            "desktop.ini"
        );

        createFile(
            root,
            "cropinfrared",
            "notes.txt"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (!pairs.empty()) {
            return 15;
        }
    }

    // ------------------------------------------------------------
    // Invalid supported-file stem is a hard pairing error.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCanonicalLayout(
            root
        );

        createFile(
            root,
            "crop_LR_visible",
            "scene_00001.jpg"
        );

        createFile(
            root,
            "cropinfrared",
            "scene_00001.jpg"
        );

        try {
            static_cast<void>(
                adapter.discoverPairs(
                    root,
                    DatasetPartitionId("all")
                )
            );

            return 16;
        }
        catch (const DatasetPairingError&) {
        }
    }

    // ------------------------------------------------------------
    // Important: zero is syntactically valid.
    // Python freezes digits, not positive integer identity.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCanonicalLayout(
            root
        );

        createFile(
            root,
            "crop_LR_visible",
            "FLIR_00000.jpg"
        );

        createFile(
            root,
            "cropinfrared",
            "FLIR_00000.jpg"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (
            pairs.size() != 1
            || pairs.at(0).pairId()
                != "roadscene_all_FLIR_00000"
        ) {
            return 17;
        }
    }

    // ------------------------------------------------------------
    // Unsupported partition must fail.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCanonicalLayout(
            temporary.path()
        );

        try {
            static_cast<void>(
                adapter.discoverPairs(
                    temporary.path(),
                    DatasetPartitionId(
                        "test"
                    )
                )
            );

            return 18;
        }
        catch (const DatasetPairingError&) {
        }
    }

    return 0;
}