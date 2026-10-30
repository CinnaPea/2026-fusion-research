#include "core/datasets/dvtod_adapter.h"

#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using qart::core::datasets::DVTODAdapter;
using qart::core::domain::datasets::DatasetLayoutError;
using qart::core::domain::datasets::DatasetPairingError;
using qart::core::domain::datasets::DatasetPartitionId;

void writeFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << "test";
}

void createLayout(const std::filesystem::path& root)
{
    for (const std::string& modality : {"vis1", "ir1"}) {
        for (const std::string& partition : {"train", "val"}) {
            std::filesystem::create_directories(
                root / "raw" / "dvtdo" / modality / "images" / partition
            );
        }
    }
}

} // namespace

int main()
{
    DVTODAdapter adapter;

    if (adapter.datasetId() != "dvtod") {
        return 1;
    }

    const auto partitions = adapter.supportedPartitions();

    if (
        partitions.size() != 2
        || partitions.at(0) != DatasetPartitionId("train")
        || partitions.at(1) != DatasetPartitionId("val")
    ) {
        return 2;
    }

    const auto root =
        std::filesystem::temp_directory_path()
        / "qart_dvtod_adapter_contract";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    bool missingLayoutRejected = false;

    try {
        adapter.validateLayout(root);
    }
    catch (const DatasetLayoutError&) {
        missingLayoutRejected = true;
    }

    if (!missingLayoutRejected) {
        std::filesystem::remove_all(root);
        return 3;
    }

    createLayout(root);

    for (const std::string& stem : {"10", "2", "1"}) {
        writeFile(
            root / "raw/dvtdo/vis1/images/train" / (stem + ".jpg")
        );
        writeFile(
            root / "raw/dvtdo/ir1/images/train" / (stem + ".jpg")
        );
    }

    writeFile(root / "raw/dvtdo/vis1/images/train/notes.txt");
    writeFile(root / "raw/dvtdo/ir1/images/train/preview.png");

    auto trainPairs = adapter.discoverPairs(
        root,
        DatasetPartitionId("train")
    );

    if (trainPairs.size() != 3) {
        std::filesystem::remove_all(root);
        return 4;
    }

    if (
        trainPairs.at(0).pairId() != "dvtod_train_1"
        || trainPairs.at(1).pairId() != "dvtod_train_2"
        || trainPairs.at(2).pairId() != "dvtod_train_10"
    ) {
        std::filesystem::remove_all(root);
        return 5;
    }

    if (
        trainPairs.at(0).visibleRelativePath()
        != "raw/dvtdo/vis1/images/train/1.jpg"
    ) {
        std::filesystem::remove_all(root);
        return 6;
    }

    if (
        trainPairs.at(0).thermalRelativePath()
        != "raw/dvtdo/ir1/images/train/1.jpg"
    ) {
        std::filesystem::remove_all(root);
        return 7;
    }

    writeFile(root / "raw/dvtdo/vis1/images/val/3.jpg");
    writeFile(root / "raw/dvtdo/ir1/images/val/3.jpg");

    const auto valPairs = adapter.discoverPairs(
        root,
        DatasetPartitionId("val")
    );

    if (
        valPairs.size() != 1
        || valPairs.front().pairId() != "dvtod_val_3"
    ) {
        std::filesystem::remove_all(root);
        return 8;
    }

    writeFile(root / "raw/dvtdo/vis1/images/train/20.jpg");

    trainPairs = adapter.discoverPairs(
        root,
        DatasetPartitionId("train")
    );

    if (
        trainPairs.size() != 4
        || trainPairs.back().pairId() != "dvtod_train_20"
        || trainPairs.back().thermalRelativePath()
            != "raw/dvtdo/ir1/images/train/20.jpg"
    ) {
        std::filesystem::remove_all(root);
        return 9;
    }

    bool unsupportedPartitionRejected = false;

    try {
        static_cast<void>(
            adapter.discoverPairs(
                root,
                DatasetPartitionId("test")
            )
        );
    }
    catch (const DatasetPairingError&) {
        unsupportedPartitionRejected = true;
    }

    std::filesystem::remove_all(root);

    if (!unsupportedPartitionRejected) {
        return 10;
    }

    return 0;
}
