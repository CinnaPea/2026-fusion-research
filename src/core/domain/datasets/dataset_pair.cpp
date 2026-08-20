//
// Created by hakgu on 8/11/2026.
//

#include "dataset_pair.h"
#include <cctype>
#include <stdexcept>
#include <utility>

namespace qart::core::domain::datasets {
    namespace {
        bool isAsciiWhitespace(const char character) noexcept
        {
            return character == ' '
                || character == '\t'
                || character == '\n'
                || character == '\r'
                || character == '\f'
                || character == '\v';
        }

        bool isWhitespaceOnly(const std::string& value) noexcept
        {
            for (const char character : value) {
                if (!isAsciiWhitespace(character)) {
                    return false;
                }
            }

            return true;
        }

        bool isWindowsDriveAbsolute(
            const std::string& path
        ) noexcept
        {
            if (path.size() < 3) {
                return false;
            }

            const unsigned char driveCharacter =
                static_cast<unsigned char>(path[0]);

            const bool hasDriveLetter =
                std::isalpha(driveCharacter) != 0;

            return hasDriveLetter
                && path[1] == ':'
                && (path[2] == '/' || path[2] == '\\');
        }

        bool containsParentTraversal(
            const std::string& path
        ) noexcept
        {
            std::size_t segmentStart = 0;

            while (segmentStart <= path.size()) {
                const std::size_t separator =
                    path.find('/', segmentStart);

                const std::size_t segmentLength =
                    separator == std::string::npos
                        ? path.size() - segmentStart
                        : separator - segmentStart;

                if (
                    segmentLength == 2
                    && path.compare(
                        segmentStart,
                        segmentLength,
                        ".."
                    ) == 0
                ) {
                    return true;
                }

                if (separator == std::string::npos) {
                    break;
                }

                segmentStart = separator + 1;
            }

            return false;
        }
    }
    DatasetPair::DatasetPair(
        std::string pairId,
        std::string datasetId,
        DatasetPartitionId partitionId,
        std::string sourceStem,
        std::string visibleRelativePath,
        std::string thermalRelativePath
    )
        : pairId_(std::move(pairId)),
          datasetId_(std::move(datasetId)),
          partitionId_(std::move(partitionId)),
          sourceStem_(std::move(sourceStem)),
          visibleRelativePath_(std::move(visibleRelativePath)),
          thermalRelativePath_(std::move(thermalRelativePath)) {
        validateRequiredText(
            pairId_,
            "Pair ID"
        );

        validateRequiredText(
            datasetId_,
            "Dataset ID"
        );

        validateRequiredText(
            sourceStem_,
            "source_stem"
        );

        validateRelativeManifestPath(
            visibleRelativePath_,
            "visible_relative_path"
        );

        validateRelativeManifestPath(
            thermalRelativePath_,
            "thermal_relative_path"
        );
    }

    const std::string& DatasetPair::pairId() const noexcept
    {
        return pairId_;
    }

    const std::string& DatasetPair::datasetId() const noexcept
    {
        return datasetId_;
    }

    const DatasetPartitionId&
    DatasetPair::partitionId() const noexcept
    {
        return partitionId_;
    }

    const std::string&
    DatasetPair::sourceStem() const noexcept
    {
        return sourceStem_;
    }

    const std::string&
    DatasetPair::visibleRelativePath() const noexcept
    {
        return visibleRelativePath_;
    }

    const std::string&
    DatasetPair::thermalRelativePath() const noexcept
    {
        return thermalRelativePath_;
    }

    void DatasetPair::validateRequiredText(
        const std::string& value,
        const char* fieldName
    )
    {
        if (value.empty() || isWhitespaceOnly(value)) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " must not be empty."
            );
        }
    }

    void DatasetPair::validateRelativeManifestPath(
        const std::string& path,
        const char* fieldName
    )
    {
        if (path.empty()) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " must not be empty."
            );
        }

        if (
            path.front() == '/'
            || isWindowsDriveAbsolute(path)
        ) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " must be relative to the shared dataset root."
            );
        }

        if (containsParentTraversal(path)) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " must not escape the shared dataset root."
            );
        }

        if (path.find('\\') != std::string::npos) {
            throw std::invalid_argument(
                std::string(fieldName)
                + " must use forward slashes."
            );
        }
    }

    bool operator==(
        const DatasetPair& left,
        const DatasetPair& right
    ) noexcept
    {
        return left.pairId_ == right.pairId_
            && left.datasetId_ == right.datasetId_
            && left.partitionId_ == right.partitionId_
            && left.sourceStem_ == right.sourceStem_
            && left.visibleRelativePath_
                == right.visibleRelativePath_
            && left.thermalRelativePath_
                == right.thermalRelativePath_;
    }

    bool operator!=(
        const DatasetPair& left,
        const DatasetPair& right
    ) noexcept
    {
        return !(left == right);
    }

}