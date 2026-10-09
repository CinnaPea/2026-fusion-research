#include "core/datasets/tno_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::datasets::TNOAdapter;

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
              / "qart_tno_adapter_contract"
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
            std::string("vi"),
            std::string("ir")
        }
    ) {
        std::filesystem::create_directories(
            root
            / "raw"
            / "tno"
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
        / "tno"
        / directoryName
        / filename,
        std::ios::binary
    );
}

} // namespace


int main()
{
    TNOAdapter adapter;

    // ------------------------------------------------------------
    // Native identity: exactly one "all" partition.
    // ------------------------------------------------------------

    if (
        adapter.datasetId()
        != "tno"
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
    // Canonical vi / ir layout is sufficient.
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
            / "tno"
            / "vi"
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
                std::string("003.png"),
                std::string("001.png"),
                std::string("002.png")
            }
        ) {
            createFile(
                root,
                "vi",
                filename
            );

            createFile(
                root,
                "ir",
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
            != "tno_all_001"
        ) {
            return 8;
        }

        if (
            pairs.at(1).pairId()
            != "tno_all_002"
        ) {
            return 9;
        }

        if (
            pairs.at(2).pairId()
            != "tno_all_003"
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
            "vi",
            "001.png"
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
            "raw/tno/vi/001.png"
        ) {
            return 13;
        }

        if (
            pairs.at(0).thermalRelativePath()
            !=
            "raw/tno/ir/001.png"
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
            "vi",
            "notes.txt"
        );

        createFile(
            root,
            "ir",
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
    // No three-digit filename contract:
    // arbitrary unique PNG stems remain valid candidates.
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
            "vi",
            "scene_alpha.png"
        );

        createFile(
            root,
            "ir",
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
                != "tno_all_scene_alpha"
        ) {
            return 16;
        }
    }

    // ------------------------------------------------------------
    // Ordinary native numeric stem remains canonical.
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
            "vi",
            "001.png"
        );

        createFile(
            root,
            "ir",
            "001.png"
        );

        const auto pairs =
            adapter.discoverPairs(
                root,
                DatasetPartitionId("all")
            );

        if (
            pairs.size() != 1
            || pairs.at(0).pairId()
                != "tno_all_001"
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