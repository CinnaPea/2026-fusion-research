#include "core/datasets/canonical_pair_lookup.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using qart::core::datasets::CanonicalPairLookup;
using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;

DatasetPair pair(
    const std::string& id,
    const std::string& dataset,
    const std::string& partition,
    const std::string& stem,
    const std::string& visible,
    const std::string& thermal
)
{
    return DatasetPair(id, dataset, DatasetPartitionId(partition), stem, visible, thermal);
}

void touch(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path).put('\n');
}

} // namespace

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "qart_canonical_pair_lookup_contract";
    std::filesystem::remove_all(root);

    const std::vector<DatasetPair> pairs = {
        pair("llvip_train_010001", "llvip", "train", "010001",
             "raw/llvip/visible/train/010001.jpg", "raw/llvip/infrared/train/010001.jpg"),
        pair("msrs_test_0001", "msrs", "test", "0001",
             "raw/msrs/test/vi/0001.png", "raw/msrs/test/ir/0001.png"),
        pair("roadscene_all_FLIR_00001", "roadscene", "all", "FLIR_00001",
             "raw/roadscene/crop_LR_visible/FLIR_00001.jpg", "raw/roadscene/cropinfrared/FLIR_00001.jpg")
    };

    const std::vector<std::pair<std::string, std::string>> canonicalCases = {
        {"raw/llvip/visible/train/010001.jpg", "llvip_train_010001"},
        {"raw/llvip/infrared/train/010001.jpg", "llvip_train_010001"},
        {"raw/msrs/test/vi/0001.png", "msrs_test_0001"},
        {"raw/msrs/test/ir/0001.png", "msrs_test_0001"},
        {"raw/roadscene/crop_LR_visible/FLIR_00001.jpg", "roadscene_all_FLIR_00001"},
        {"raw/roadscene/cropinfrared/FLIR_00001.jpg", "roadscene_all_FLIR_00001"}
    };

    for (const auto& [relative, expectedId] : canonicalCases) {
        touch(root / relative);
        const auto found = CanonicalPairLookup::findByImagePath(root, root / relative, pairs);
        if (!found.has_value() || found->pairId() != expectedId) {
            std::filesystem::remove_all(root);
            return 1;
        }
    }

    for (const std::string& auxiliary : {
             "raw/roadscene/infrared/FLIR_00001.jpg",
             "raw/roadscene/crop_HR_visible/FLIR_00001.jpg"
         }) {
        touch(root / auxiliary);
        if (CanonicalPairLookup::findByImagePath(root, root / auxiliary, pairs).has_value()) {
            std::filesystem::remove_all(root);
            return 2;
        }
    }

    std::filesystem::remove_all(root);
    return 0;
}
