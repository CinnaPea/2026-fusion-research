//
// Created by hakgu on 8/19/2026.
//

#include "core/datasets/dataset_adapter.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using qart::core::datasets::DatasetAdapter;

using qart::core::domain::datasets::
    DatasetPair;
using qart::core::domain::datasets::
    DatasetPartitionId;

class StubDatasetAdapter final
    : public DatasetAdapter
{
public:
    [[nodiscard]]
    std::string datasetId() const override
    {
        return "stub";
    }

    [[nodiscard]]
    std::vector<DatasetPartitionId>
    supportedPartitions() const override
    {
        return {
            DatasetPartitionId("train")
        };
    }

    void validateLayout(
        const std::filesystem::path& datasetRoot
    ) const override
    {
        static_cast<void>(
            datasetRoot
        );
    }

    [[nodiscard]]
    std::vector<DatasetPair>
    discoverPairs(
        const std::filesystem::path& datasetRoot,
        const DatasetPartitionId& partitionId
    ) const override
    {
        static_cast<void>(
            datasetRoot
        );

        if (
            !supportsPartition(
                partitionId
            )
        ) {
            return {};
        }

        return {
            DatasetPair(
                "stub_train_000001",
                "stub",
                partitionId,
                "000001",
                "raw/stub/visible/train/000001.jpg",
                "raw/stub/thermal/train/000001.jpg"
            )
        };
    }
};

} // namespace

int main()
{
    // ------------------------------------------------------------
    // Generic adapter remains abstract.
    // ------------------------------------------------------------

    static_assert(
        std::is_abstract_v<
            DatasetAdapter
        >
    );

    StubDatasetAdapter adapter;

    // ------------------------------------------------------------
    // Stable dataset identity.
    // ------------------------------------------------------------

    if (
        adapter.datasetId()
        != "stub"
    ) {
        return 1;
    }

    // ------------------------------------------------------------
    // Canonical native partitions.
    // ------------------------------------------------------------

    const auto partitions =
        adapter.supportedPartitions();

    if (partitions.size() != 1) {
        return 2;
    }

    if (
        partitions.at(0)
        != DatasetPartitionId("train")
    ) {
        return 3;
    }

    // ------------------------------------------------------------
    // Typed membership.
    // ------------------------------------------------------------

    if (
        !adapter.supportsPartition(
            DatasetPartitionId("train")
        )
    ) {
        return 4;
    }

    if (
        adapter.supportsPartition(
            DatasetPartitionId("test")
        )
    ) {
        return 5;
    }

    // ------------------------------------------------------------
    // Textual canonical membership.
    // ------------------------------------------------------------

    if (
        !adapter.supportsPartition(
            std::string("train")
        )
    ) {
        return 6;
    }

    if (
        adapter.supportsPartition(
            std::string("test")
        )
    ) {
        return 7;
    }

    // ------------------------------------------------------------
    // Explicit dataset root accepted by concrete adapter.
    // ------------------------------------------------------------

    adapter.validateLayout(
        std::filesystem::path(
            "F:/QART_Datasets"
        )
    );

    // ------------------------------------------------------------
    // Canonical typed discovery.
    // ------------------------------------------------------------

    const auto pairs =
        adapter.discoverPairs(
            std::filesystem::path(
                "F:/QART_Datasets"
            ),
            DatasetPartitionId("train")
        );

    if (pairs.size() != 1) {
        return 8;
    }

    if (
        pairs.at(0).pairId()
        != "stub_train_000001"
    ) {
        return 9;
    }

    if (
        pairs.at(0).datasetId()
        != "stub"
    ) {
        return 10;
    }

    if (
        pairs.at(0).partitionId()
        != DatasetPartitionId("train")
    ) {
        return 11;
    }

    // ------------------------------------------------------------
    // Unsupported behavior is owned by concrete adapter.
    // Stub chooses an empty result.
    // ------------------------------------------------------------

    const auto unsupported =
        adapter.discoverPairs(
            std::filesystem::path(
                "F:/QART_Datasets"
            ),
            DatasetPartitionId("test")
        );

    if (!unsupported.empty()) {
        return 12;
    }

    return 0;
}