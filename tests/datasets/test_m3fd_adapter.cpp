#include "core/datasets/m3fd_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::datasets::M3FDAdapter;

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
              / "qart_m3fd_adapter_contract"
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
            std::string("Vis"),
            std::string("Ir")
        }
    ) {
        std::filesystem::create_directories(
            root
            / "raw"
            / "m3fd"
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
        / "m3fd"
        / directoryName
        / filename,
        std::ios::binary
    );
}

} // namespace


int main()
{
    M3FDAdapter adapter;

    // ------------------------------------------------------------
    // Native identity: exactly one "all" partition.
    // ------------------------------------------------------------

    if (
        adapter.datasetId()
        != "m3fd"
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
        adapter.supportsPartition("train")
    ) {
        return 5;
    }

    // ------------------------------------------------------------
    // Canonical Vis / Ir layout is sufficient.
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
            / "m3fd"
            / "Vis"
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
    // PNG stems are paired and sorted deterministically.
    // ------------------------------------------------------------

    {
        TemporaryDirectory temporary;

        const auto root =
            temporary.path();

        createCanonicalLayout(
            root
        );

        for (
            const std::string filename
            : {
                std::string("00025.png"),
                std::string("00000.png"),
                std::string("00011.png")
            }
        ) {
            createFile(
                root,
                "Vis",
                filename
            );

            createFile(
                root,
                "Ir",
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
            != "m3fd_all_00000"
        ) {
            return 8;
        }

        if (
            pairs.at(1).pairId()
            != "m3fd_all_00011"
        ) {
            return 9;
        }

        if (
            pairs.at(2).pairId()
            != "m3fd_all_00025"
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
            "Vis",
            "00000.png"
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
            "raw/m3fd/Vis/00000.png"
        ) {
            return 13;
        }

        if (
            pairs.at(0).thermalRelativePath()
            !=
            "raw/m3fd/Ir/00000.png"
        ) {
            return 14;
        }
    }

    // ------------------------------------------------------------
    // Unsupported files are ignored.
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
            "Vis",
            "notes.txt"
        );

        createFile(
            root,
            "Ir",
            "preview.jpg"
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
    // No filename-regex contract:
    // arbitrary unique PNG stems remain canonical candidates.
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
            "Vis",
            "scene_alpha.png"
        );

        createFile(
            root,
            "Ir",
            "scene_alpha.png"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (
            pairs.size() != 1
            || pairs.at(0).pairId()
                != "m3fd_all_scene_alpha"
        ) {
            return 16;
        }
    }

    // ------------------------------------------------------------
    // Native zero-valued stem remains valid.
    // The real M3FD package begins with 00000.
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
            "Vis",
            "00000.png"
        );

        createFile(
            root,
            "Ir",
            "00000.png"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (
            pairs.size() != 1
            || pairs.at(0).pairId()
                != "m3fd_all_00000"
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
                        "train"
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