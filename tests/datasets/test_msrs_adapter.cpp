//
// Created by hakgu on 8/20/2026.
//

#include "core/datasets/msrs_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::datasets::MSRSAdapter;

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
              / "qart_msrs_adapter_contract"
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

void createCompleteLayout(
    const std::filesystem::path& root
)
{
    for (
        const std::string partition
        : {
            std::string("train"),
            std::string("test")
        }
    ) {
        for (
            const std::string modality
            : {
                std::string("vi"),
                std::string("ir")
            }
        ) {
            std::filesystem::create_directories(
                root
                / "raw"
                / "msrs"
                / partition
                / modality
            );
        }
    }
}

void createFile(
    const std::filesystem::path& root,
    const std::string& partition,
    const std::string& modality,
    const std::string& filename
)
{
    std::ofstream stream(
        root
        / "raw"
        / "msrs"
        / partition
        / modality
        / filename,
        std::ios::binary
    );
}

bool portablePath(
    const std::string& value
)
{
    return
        !value.empty()
        && value.find('\\')
            == std::string::npos
        && !std::filesystem::path(
            value
        ).is_absolute();
}

} // namespace

int main()
{
    MSRSAdapter adapter;

    // ------------------------------------------------------------
    // Identity and canonical partitions.
    // ------------------------------------------------------------

    if (adapter.datasetId() != "msrs") {
        return 1;
    }

    const auto partitions =
        adapter.supportedPartitions();

    if (partitions.size() != 2) {
        return 2;
    }

    if (
        partitions.at(0)
            != DatasetPartitionId("train")
        || partitions.at(1)
            != DatasetPartitionId("test")
    ) {
        return 3;
    }

    // ------------------------------------------------------------
    // Missing root.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        try {
            adapter.validateLayout(
                temporary.path()
                / "missing"
            );

            return 4;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Complete layout accepted.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCompleteLayout(
            temporary.path()
        );

        adapter.validateLayout(
            temporary.path()
        );
    }

    // ------------------------------------------------------------
    // Missing required directory.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCompleteLayout(
            temporary.path()
        );

        std::filesystem::remove(
            temporary.path()
            / "raw"
            / "msrs"
            / "test"
            / "ir"
        );

        try {
            adapter.validateLayout(
                temporary.path()
            );

            return 5;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Deterministic day/night discovery.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        for (
            const std::string filename
            : {
                std::string("00003N.png"),
                std::string("00001D.png")
            }
        ) {
            createFile(
                root,
                "train",
                "vi",
                filename
            );

            createFile(
                root,
                "train",
                "ir",
                filename
            );
        }

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (pairs.size() != 2) {
            return 6;
        }

        if (
            pairs.at(0).pairId()
            != "msrs_train_00001D"
        ) {
            return 7;
        }

        if (
            pairs.at(1).pairId()
            != "msrs_train_00003N"
        ) {
            return 8;
        }
    }

    // ------------------------------------------------------------
    // Visible-only candidate retained.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        createFile(
            root,
            "train",
            "vi",
            "00001D.png"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (pairs.size() != 1) {
            return 9;
        }

        if (
            pairs.at(0).visibleRelativePath()
            != "raw/msrs/train/vi/00001D.png"
        ) {
            return 10;
        }

        if (
            pairs.at(0).thermalRelativePath()
            != "raw/msrs/train/ir/00001D.png"
        ) {
            return 11;
        }
    }

    // ------------------------------------------------------------
    // Thermal-only candidate retained.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        createFile(
            root,
            "test",
            "ir",
            "00004N.png"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("test")
            );

        if (pairs.size() != 1) {
            return 12;
        }

        if (
            pairs.at(0).pairId()
            != "msrs_test_00004N"
        ) {
            return 13;
        }

        if (
            pairs.at(0).visibleRelativePath()
            != "raw/msrs/test/vi/00004N.png"
        ) {
            return 14;
        }
    }

    // ------------------------------------------------------------
    // Unsupported extensions ignored.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        createFile(
            root,
            "train",
            "vi",
            "00001D.jpg"
        );

        createFile(
            root,
            "train",
            "ir",
            "00001D.jpg"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (!pairs.empty()) {
            return 15;
        }
    }

    // ------------------------------------------------------------
    // Segmentation_labels does not participate.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        const auto labelDirectory =
            root
            / "raw"
            / "msrs"
            / "train"
            / "Segmentation_labels";

        std::filesystem::create_directories(
            labelDirectory
        );

        std::ofstream(
            labelDirectory
            / "00001D.png"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (!pairs.empty()) {
            return 16;
        }
    }

    // ------------------------------------------------------------
    // Portable paths.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        createFile(
            root,
            "test",
            "vi",
            "00004N.png"
        );

        createFile(
            root,
            "test",
            "ir",
            "00004N.png"
        );

        const auto pair =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("test")
            ).at(0);

        if (
            !portablePath(
                pair.visibleRelativePath()
            )
            || !portablePath(
                pair.thermalRelativePath()
            )
        ) {
            return 17;
        }
    }

    // ------------------------------------------------------------
    // Empty valid partition.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCompleteLayout(
            temporary.path()
        );

        if (
            !adapter.discoverPairs(
                temporary.path(),
                DatasetPartitionId("test")
            ).empty()
        ) {
            return 18;
        }
    }

    // ------------------------------------------------------------
    // Missing D/N suffix rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        createFile(
            root,
            "train",
            "vi",
            "00001.png"
        );

        createFile(
            root,
            "train",
            "ir",
            "00001.png"
        );

        try {
            static_cast<void>(
                adapter.discoverPairs(
                    root,
                    DatasetPartitionId("train")
                )
            );

            return 19;
        }
        catch (const DatasetPairingError&) {
        }
    }

    // ------------------------------------------------------------
    // Zero numeric identity rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(root);

        createFile(
            root,
            "train",
            "vi",
            "00000D.png"
        );

        createFile(
            root,
            "train",
            "ir",
            "00000D.png"
        );

        try {
            static_cast<void>(
                adapter.discoverPairs(
                    root,
                    DatasetPartitionId("train")
                )
            );

            return 20;
        }
        catch (const DatasetPairingError&) {
        }
    }

    // ------------------------------------------------------------
    // Unsupported native partition.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCompleteLayout(
            temporary.path()
        );

        try {
            static_cast<void>(
                adapter.discoverPairs(
                    temporary.path(),
                    DatasetPartitionId(
                        "detection"
                    )
                )
            );

            return 21;
        }
        catch (const DatasetPairingError&) {
        }
    }

    return 0;
}