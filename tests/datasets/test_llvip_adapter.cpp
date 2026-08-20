//
// Created by hakgu on 8/19/2026.
//

#include "core/datasets/llvip_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using qart::core::datasets::LLVIPAdapter;

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
              / "qart_llvip_adapter_contract"
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
        const std::string modality
        : {
            std::string("visible"),
            std::string("infrared")
        }
    ) {
        for (
            const std::string partition
            : {
                std::string("train"),
                std::string("test")
            }
        ) {
            std::filesystem::create_directories(
                root
                / "raw"
                / "llvip"
                / modality
                / partition
            );
        }
    }
}

void createFile(
    const std::filesystem::path& root,
    const std::string& modality,
    const std::string& partition,
    const std::string& filename
)
{
    const auto path =
        root
        / "raw"
        / "llvip"
        / modality
        / partition
        / filename;

    std::ofstream stream(
        path,
        std::ios::binary
    );
}

bool isPortableRelativePath(
    const std::string& path
)
{
    if (path.empty()) {
        return false;
    }

    if (
        path.find('\\')
        != std::string::npos
    ) {
        return false;
    }

    if (
        std::filesystem::path(
            path
        ).is_absolute()
    ) {
        return false;
    }

    return true;
}

} // namespace

int main()
{
    LLVIPAdapter adapter;

    // ------------------------------------------------------------
    // Identity and canonical partitions.
    // ------------------------------------------------------------

    if (
        adapter.datasetId()
        != "llvip"
    ) {
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

    if (
        !adapter.supportsPartition("train")
        || !adapter.supportsPartition("test")
    ) {
        return 4;
    }

    // ------------------------------------------------------------
    // Missing shared root.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto missingRoot =
            temporary.path()
            / "missing";

        try {
            adapter.validateLayout(
                missingRoot
            );

            return 5;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Shared root is a file.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto rootFile =
            temporary.path()
            / "root.txt";

        std::ofstream stream(
            rootFile
        );

        stream.close();

        try {
            adapter.validateLayout(
                rootFile
            );

            return 6;
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
    // Missing required directory rejected.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        createCompleteLayout(
            temporary.path()
        );

        std::filesystem::remove(
            temporary.path()
            / "raw"
            / "llvip"
            / "infrared"
            / "test"
        );

        try {
            adapter.validateLayout(
                temporary.path()
            );

            return 7;
        }
        catch (const DatasetLayoutError&) {
        }
    }

    // ------------------------------------------------------------
    // Exact filename pairs sorted deterministically.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(
            root
        );

        for (
            const std::string filename
            : {
                std::string("010002.jpg"),
                std::string("010001.jpg")
            }
        ) {
            createFile(
                root,
                "visible",
                "train",
                filename
            );

            createFile(
                root,
                "infrared",
                "train",
                filename
            );
        }

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (pairs.size() != 2) {
            return 8;
        }

        if (
            pairs.at(0).pairId()
            != "llvip_train_010001"
        ) {
            return 9;
        }

        if (
            pairs.at(1).pairId()
            != "llvip_train_010002"
        ) {
            return 10;
        }
    }

    // ------------------------------------------------------------
    // Visible-only candidate retained.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(
            root
        );

        createFile(
            root,
            "visible",
            "train",
            "010001.jpg"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (pairs.size() != 1) {
            return 11;
        }

        if (
            pairs.at(0)
                .visibleRelativePath()
            != "raw/llvip/visible/train/010001.jpg"
        ) {
            return 12;
        }

        if (
            pairs.at(0)
                .thermalRelativePath()
            != "raw/llvip/infrared/train/010001.jpg"
        ) {
            return 13;
        }
    }

    // ------------------------------------------------------------
    // Thermal-only candidate retained.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(
            root
        );

        createFile(
            root,
            "infrared",
            "test",
            "190001.jpg"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("test")
            );

        if (pairs.size() != 1) {
            return 14;
        }

        if (
            pairs.at(0).pairId()
            != "llvip_test_190001"
        ) {
            return 15;
        }

        if (
            pairs.at(0)
                .visibleRelativePath()
            != "raw/llvip/visible/test/190001.jpg"
        ) {
            return 16;
        }

        if (
            pairs.at(0)
                .thermalRelativePath()
            != "raw/llvip/infrared/test/190001.jpg"
        ) {
            return 17;
        }
    }

    // ------------------------------------------------------------
    // Unsupported formats ignored.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(
            root
        );

        createFile(
            root,
            "visible",
            "train",
            "010001.png"
        );

        createFile(
            root,
            "infrared",
            "train",
            "010001.png"
        );

        createFile(
            root,
            "visible",
            "train",
            "notes.txt"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (!pairs.empty()) {
            return 18;
        }
    }

    // ------------------------------------------------------------
    // Manifest paths are portable relative paths.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(
            root
        );

        createFile(
            root,
            "visible",
            "train",
            "010001.jpg"
        );

        createFile(
            root,
            "infrared",
            "train",
            "010001.jpg"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("train")
            );

        if (pairs.size() != 1) {
            return 19;
        }

        if (
            !isPortableRelativePath(
                pairs.at(0)
                    .visibleRelativePath()
            )
        ) {
            return 20;
        }

        if (
            !isPortableRelativePath(
                pairs.at(0)
                    .thermalRelativePath()
            )
        ) {
            return 21;
        }
    }

    // ------------------------------------------------------------
    // Empty valid partition returns empty vector.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCompleteLayout(
            root
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("test")
            );

        if (!pairs.empty()) {
            return 22;
        }
    }

    // ------------------------------------------------------------
    // Unsupported partition is a pairing error.
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
                        "validation"
                    )
                )
            );

            return 23;
        }
        catch (const DatasetPairingError&) {
        }
    }

    return 0;
}