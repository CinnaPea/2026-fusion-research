//
// Created by hakgu on 10/16/2026.
//

// Justification & Paper Alignment
// Choice: compare C++ results with actual Python reference bytes.
// Classification: Engineering Decision — not paper-derived.
// Rule: require exact uint8 equality; missing/truncated fixtures fail.
// No scientific logic or image-quality criterion is introduced.

#include "core/enhancement/thermal_input.h"

#include <opencv2/core.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

cv::Mat readImageBytes(
    const std::filesystem::path& path,
    int rows,
    int columns,
    int channels)
{
    cv::Mat image(
        rows,
        columns,
        CV_MAKETYPE(CV_8U, channels)
    );

    const auto byteCount = static_cast<std::streamsize>(
        image.total() * image.elemSize()
    );

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw std::runtime_error(
            "Cannot open fixture: " + path.string()
        );
    }

    file.read(
        reinterpret_cast<char*>(image.data),
        byteCount
    );

    if (file.gcount() != byteCount) {
        throw std::runtime_error(
            "Truncated fixture: " + path.string()
        );
    }

    if (file.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error(
            "Unexpected extra bytes: " + path.string()
        );
    }

    return image;
}

void requireExact(
    const cv::Mat& actual,
    const cv::Mat& expected,
    const std::string& context)
{
    if (
        actual.dims != 2
        || actual.type() != expected.type()
        || actual.size() != expected.size()
    ) {
        throw std::runtime_error(
            context + ": output type/geometry mismatch"
        );
    }

    const double maximumDifference =
        cv::norm(actual, expected, cv::NORM_INF);

    if (maximumDifference != 0.0) {
        throw std::runtime_error(
            context
            + ": pixel mismatch; maximum difference = "
            + std::to_string(maximumDifference)
        );
    }
}

} // namespace

int main(int argc, char** argv)
{
    using namespace qart::core::enhancement;

    try {
        if (argc != 2) {
            throw std::runtime_error(
                "Usage: test_phase2_thermal_input_parity "
                "<fixture-directory>"
            );
        }

        const std::filesystem::path directory(argv[1]);
        std::ifstream manifest(directory / "cases.txt");

        if (!manifest) {
            throw std::runtime_error(
                "Cannot open fixture manifest"
            );
        }

        std::string contract;
        int caseCount = 0;

        if (
            !(manifest >> contract >> caseCount)
            || contract != "P2_THERMAL_INPUT_V1"
            || caseCount != 12
        ) {
            throw std::runtime_error(
                "Incorrect fixture contract or case count"
            );
        }

        for (int index = 0; index < caseCount; ++index) {
            std::string caseId;
            int rows = 0;
            int columns = 0;
            int channels = 0;

            if (
                !(manifest
                    >> caseId
                    >> rows
                    >> columns
                    >> channels)
            ) {
                throw std::runtime_error(
                    "Incomplete fixture manifest"
                );
            }

            const bool supportedShape =
                (rows == 1 && columns == 1)
                || (rows == 1 && columns == 17)
                || (rows == 19 && columns == 1)
                || (rows == 31 && columns == 33);

            const std::string expectedId =
                "input_" + std::to_string(rows)
                + "x" + std::to_string(columns)
                + "_c" + std::to_string(channels);

            if (
                !supportedShape
                || (channels != 1
                    && channels != 3
                    && channels != 4)
                || caseId != expectedId
            ) {
                throw std::runtime_error(
                    "Invalid fixture manifest entry"
                );
            }

            const cv::Mat input = readImageBytes(
                directory / (caseId + "_input.bin"),
                rows, columns, channels
            );

            const cv::Mat expectedPrepared = readImageBytes(
                directory / (caseId + "_prepared.bin"),
                rows, columns, 1
            );

            const cv::Mat expectedBypass = readImageBytes(
                directory / (caseId + "_bypass.bin"),
                rows, columns, 1
            );

            const cv::Mat original = input.clone();

            const cv::Mat prepared =
                prepareThermalUint8Plane(input);

            requireExact(
                prepared,
                expectedPrepared,
                caseId + " preparation"
            );

            const cv::Mat bypassed =
                bypassThermal(prepared);

            requireExact(
                bypassed,
                expectedBypass,
                caseId + " BYPASS"
            );

            requireExact(
                input,
                original,
                caseId + " source preservation"
            );

            std::cout << caseId << ": exact\n";
        }

        std::string unexpected;
        if (manifest >> unexpected) {
            throw std::runtime_error(
                "Unexpected trailing manifest entry"
            );
        }

        std::cout
            << "12/12 cases exact: "
            << "preparation and BYPASS\n"
            << "C++ OpenCV: " << CV_VERSION << '\n';

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}