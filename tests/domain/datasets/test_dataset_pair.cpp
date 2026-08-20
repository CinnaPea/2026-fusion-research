//
// Created by hakgu on 8/11/2026.
//

#include "core/domain/datasets/dataset_pair.h"

#include <stdexcept>
#include <string>

namespace {

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;

DatasetPair makeLlVipPair()
{
    return DatasetPair(
        "llvip_train_010001",
        "llvip",
        DatasetPartitionId("train"),
        "010001",
        "raw/llvip/visible/train/010001.jpg",
        "raw/llvip/infrared/train/010001.jpg"
    );
}

bool rejectsPair(
    const std::string& pairId,
    const std::string& datasetId,
    const std::string& sourceStem,
    const std::string& visiblePath,
    const std::string& thermalPath
)
{
    try {
        const DatasetPair pair(
            pairId,
            datasetId,
            DatasetPartitionId("train"),
            sourceStem,
            visiblePath,
            thermalPath
        );

        static_cast<void>(pair);
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const DatasetPair llvip = makeLlVipPair();

    if (llvip.pairId() != "llvip_train_010001") {
        return 1;
    }

    if (llvip.datasetId() != "llvip") {
        return 2;
    }

    if (
        llvip.partitionId()
        != DatasetPartitionId("train")
    ) {
        return 3;
    }

    if (llvip.sourceStem() != "010001") {
        return 4;
    }

    if (
        llvip.visibleRelativePath()
        != "raw/llvip/visible/train/010001.jpg"
    ) {
        return 5;
    }

    if (
        llvip.thermalRelativePath()
        != "raw/llvip/infrared/train/010001.jpg"
    ) {
        return 6;
    }

    const DatasetPair roadscene(
        "roadscene_all_FLIR_00006",
        "roadscene",
        DatasetPartitionId("all"),
        "FLIR_00006",
        "raw/roadscene/crop_LR_visible/FLIR_00006.jpg",
        "raw/roadscene/cropinfrared/FLIR_00006.jpg"
    );

    if (
        roadscene.partitionId()
        != DatasetPartitionId("all")
    ) {
        return 7;
    }

    if (makeLlVipPair() != makeLlVipPair()) {
        return 8;
    }

    if (
        !rejectsPair(
            "",
            "llvip",
            "010001",
            "raw/visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 9;
    }

    if (
        !rejectsPair(
            "pair",
            "   ",
            "010001",
            "raw/visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 10;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "\t",
            "raw/visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 11;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "010001",
            "",
            "raw/thermal.jpg"
        )
    ) {
        return 12;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "010001",
            "/absolute/visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 13;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "010001",
            "C:/datasets/visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 14;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "010001",
            "raw/../visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 15;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "010001",
            "raw\\visible.jpg",
            "raw/thermal.jpg"
        )
    ) {
        return 16;
    }

    if (
        !rejectsPair(
            "pair",
            "llvip",
            "010001",
            "raw/visible.jpg",
            "../thermal.jpg"
        )
    ) {
        return 17;
    }

    return 0;
}