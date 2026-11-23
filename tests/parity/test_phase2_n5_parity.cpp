//
// Created by hakgu on 10/16/2026.
//

#include "core/enhancement/n5_thermal_enhancer.h"

#include <opencv2/core.hpp>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <algorithm>

namespace {

cv::Mat readPlane(
    const std::filesystem::path& path,
    int rows,
    int columns)
{
    cv::Mat plane(rows, columns, CV_8UC1);

    const auto byteCount =
        static_cast<std::streamsize>(plane.total());

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw std::runtime_error(
            "Cannot open fixture: " + path.string()
        );
    }

    file.read(
        reinterpret_cast<char*>(plane.data),
        byteCount
    );

    if (
        file.gcount() != byteCount
        || file.peek() != std::char_traits<char>::eof()
    ) {
        throw std::runtime_error(
            "Incorrect fixture size: " + path.string()
        );
    }

    return plane;
}

std::size_t countDifferences(
    const cv::Mat& actual,
    const cv::Mat& expected)
{
    if (
        actual.dims != 2
        || actual.type() != CV_8UC1
        || actual.size() != expected.size()
    ) {
        throw std::runtime_error(
            "Incorrect N5 output type or geometry"
        );
    }

    std::size_t differences = 0;

    for (int row = 0; row < actual.rows; ++row) {
        const auto* a = actual.ptr<unsigned char>(row);
        const auto* b = expected.ptr<unsigned char>(row);

        for (int column = 0; column < actual.cols; ++column) {
            if (a[column] != b[column]) {
                ++differences;
            }
        }
    }

    return differences;
}

} // namespace

int main(int argc, char** argv)
{
    using qart::core::enhancement::N5ThermalEnhancer;

    try {
        if (argc != 2) {
            throw std::runtime_error(
                "Usage: test_phase2_n5_parity "
                "<fixture-directory>"
            );
        }

        const std::filesystem::path directory(argv[1]);
        std::ifstream manifest(directory / "cases.txt");

        std::string contract;
        int caseCount = 0;

        if (!(manifest >> contract >> caseCount)) {
            throw std::runtime_error(
                "Missing N5 fixture manifest header"
            );
        }

        const bool synthetic =
            contract == "P2_N5_NUMERIC_V1" && caseCount == 12;

        const bool native =
            contract == "P2_N5_REAL_V1" && caseCount == 40;

        if (!synthetic && !native) {
            throw std::runtime_error(
                "Incorrect N5 fixture contract or case count"
            );
        }

        const std::unordered_set<std::string> expectedCases{
            "constant_0",
            "constant_1",
            "constant_127",
            "constant_255",
            "single_pixel",
            "row",
            "column",
            "ramp",
            "noise",
            "odd_size",
            "hard_edge",
            "impulse"
        };

        std::unordered_set<std::string> seen;
        N5ThermalEnhancer enhancer;

        int passed = 0;
        std::size_t totalPixelDifferences = 0;

        std::cout << std::setprecision(17);

        for (int index = 0; index < caseCount; ++index) {
            std::string caseId;
            int rows = 0;
            int columns = 0;
            double expectedRegularization = 0.0;
            int expectedConstant = 0;

            if (
                !(manifest
                    >> caseId
                    >> rows
                    >> columns
                    >> expectedRegularization
                    >> expectedConstant)
            ) {
                throw std::runtime_error(
                    "Incomplete N5 fixture manifest"
                );
            }

            const bool safeCaseId =
                !caseId.empty()
                && std::all_of(
                    caseId.begin(),
                    caseId.end(),
                    [](unsigned char character) {
                        return
                            (character >= 'a' && character <= 'z')
                            || (character >= 'A' && character <= 'Z')
                            || (character >= '0' && character <= '9')
                            || character == '_';
                    }
                );

            if (
                !safeCaseId
                || (synthetic && expectedCases.count(caseId) == 0)
                || !seen.insert(caseId).second
                || rows <= 0 || rows > 4096
                || columns <= 0 || columns > 4096
                || !std::isfinite(expectedRegularization)
                || expectedRegularization < 0.0
                || (expectedConstant != 0 && expectedConstant != 1)
            ) {
                throw std::runtime_error(
                    "Invalid N5 fixture entry"
                );
            }

            const cv::Mat input = readPlane(
                directory / (caseId + "_input.bin"),
                rows,
                columns
            );

            const cv::Mat original = input.clone();

            const cv::Mat expected = readPlane(
                directory / (caseId + "_expected.bin"),
                rows,
                columns
            );

            const auto result = enhancer.enhance(input);

            const std::size_t differences =
                countDifferences(result.outputImage, expected);

            const bool sourceUnchanged =
                countDifferences(input, original) == 0;

            const bool constantFlagMatches =
                result.constantPreserved
                    == (expectedConstant != 0);

            const bool validRegularization =
                std::isfinite(result.smallRegularization)
                    && result.smallRegularization >= 0.0;

            const bool success =
                differences == 0
                && sourceUnchanged
                && constantFlagMatches
                && validRegularization;

            totalPixelDifferences += differences;

            if (success) {
                ++passed;
            }

            std::cout
                << caseId
                << ": " << (success ? "PASS" : "FAIL")
                << "; differing_pixels=" << differences
                << "; source_unchanged=" << sourceUnchanged
                << "; constant_flag=" << constantFlagMatches
                << "; epsilon_python="
                << expectedRegularization
                << "; epsilon_cpp="
                << result.smallRegularization
                << '\n';
        }

        std::string unexpected;
        if (manifest >> unexpected) {
            throw std::runtime_error(
                "Unexpected trailing manifest entry"
            );
        }

        std::cout
            << passed << "/" << caseCount
            << " N5 cases passed; total differing pixels="
            << totalPixelDifferences << '\n'
            << "C++ OpenCV: " << CV_VERSION << '\n';

        return passed == caseCount ? 0 : 1;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}