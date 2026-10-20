//
// Created by hakgu on 10/17/2026.
//

#include "core/enhancement/n5_thermal_enhancer.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {

constexpr int Warmups = 3;
constexpr int Repeats = 10;

constexpr std::array<const char*, 9> StageNames{
    "input_validation",
    "unit_normalize",
    "unit_guided7",
    "base_quantize",
    "clahe",
    "unit_reconstruct_quantize",
    "post_normalize",
    "post_guided3",
    "post_reconstruct_quantize"
};

static_assert(
    StageNames.size()
        == static_cast<std::size_t>(
            qart::core::enhancement::N5Stage::Count
        )
);

cv::Mat readPlane(
    const std::filesystem::path& path,
    int rows,
    int columns)
{
    cv::Mat image(rows, columns, CV_8UC1);

    const auto byteCount =
        static_cast<std::streamsize>(image.total());

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

    if (
        file.gcount() != byteCount
        || file.peek() != std::char_traits<char>::eof()
    ) {
        throw std::runtime_error(
            "Incorrect fixture size: " + path.string()
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
        actual.type() != expected.type()
        || actual.size() != expected.size()
        || actual.dims != 2
    ) {
        throw std::runtime_error(
            context + ": output contract mismatch"
        );
    }

    if (cv::norm(actual, expected, cv::NORM_INF) != 0.0) {
        throw std::runtime_error(
            context + ": pixel parity failed"
        );
    }
}

} // namespace

int main(int argc, char** argv)
{
    using qart::core::enhancement::N5StageTimings;
    using qart::core::enhancement::N5ThermalEnhancer;

    try {
#ifndef NDEBUG
        throw std::runtime_error(
            "Use a Release build for stage profiling"
        );
#endif

        if (argc != 3) {
            throw std::runtime_error(
                "Usage: profile_phase2_n5_stages "
                "<native-fixture-directory> <output.csv>"
            );
        }

        const std::filesystem::path directory(argv[1]);
        const std::filesystem::path outputPath(argv[2]);

        if (std::filesystem::exists(outputPath)) {
            throw std::runtime_error(
                "Refusing to overwrite an existing report"
            );
        }

        std::ifstream manifest(directory / "cases.txt");

        std::string contract;
        int caseCount = 0;

        if (
            !(manifest >> contract >> caseCount)
            || contract != "P2_N5_REAL_V1"
            || caseCount != 40
        ) {
            throw std::runtime_error(
                "Expected the validated 40-case native packet"
            );
        }

        std::ofstream csv(outputPath);

        if (!csv) {
            throw std::runtime_error("Cannot create report");
        }

        csv << std::fixed << std::setprecision(6);
        csv << "case_id,width,height,repetition,stage,elapsed_ms\n";

        N5ThermalEnhancer enhancer;
        std::unordered_set<std::string> seen;
        int reportRows = 0;

        for (int index = 0; index < caseCount; ++index) {
            std::string caseId;
            int rows = 0;
            int columns = 0;
            double referenceEpsilon = 0.0;
            int referenceConstant = 0;

            if (
                !(manifest >> caseId >> rows >> columns
                    >> referenceEpsilon >> referenceConstant)
            ) {
                throw std::runtime_error("Incomplete manifest");
            }

            const bool safeId =
                !caseId.empty()
                && std::all_of(
                    caseId.begin(), caseId.end(),
                    [](unsigned char character) {
                        return
                            (character >= 'a' && character <= 'z')
                            || (character >= 'A' && character <= 'Z')
                            || (character >= '0' && character <= '9')
                            || character == '_';
                    }
                );

            if (
                !safeId
                || !seen.insert(caseId).second
                || rows <= 0 || rows > 4096
                || columns <= 0 || columns > 4096
                || !std::isfinite(referenceEpsilon)
                || referenceEpsilon < 0.0
                || (referenceConstant != 0
                    && referenceConstant != 1)
            ) {
                throw std::runtime_error(
                    "Invalid manifest entry"
                );
            }

            const cv::Mat input = readPlane(
                directory / (caseId + "_input.bin"),
                rows, columns
            );

            const cv::Mat original = input.clone();

            const cv::Mat expected = readPlane(
                directory / (caseId + "_expected.bin"),
                rows, columns
            );

            for (int warmup = 0; warmup < Warmups; ++warmup) {
                (void)enhancer.enhance(input);
            }

            for (int repeat = 0; repeat < Repeats; ++repeat) {
                N5StageTimings timings;
                const auto result =
                    enhancer.enhance(input, &timings);

                // Checks and CSV writing occur after internal timing.
                requireExact(
                    result.outputImage, expected, caseId
                );

                requireExact(
                    input, original, caseId + " source"
                );

                if (
                    result.constantPreserved
                        != (referenceConstant != 0)
                ) {
                    throw std::runtime_error(
                        caseId + ": constant flag mismatch"
                    );
                }

                for (
                    std::size_t stage = 0;
                    stage < StageNames.size();
                    ++stage
                ) {
                    const double elapsed =
                        timings.milliseconds[stage];

                    if (!std::isfinite(elapsed) || elapsed < 0.0) {
                        throw std::runtime_error(
                            "Invalid stage duration"
                        );
                    }

                    csv << caseId << ','
                        << columns << ',' << rows << ','
                        << repeat + 1 << ','
                        << StageNames[stage] << ','
                        << elapsed << '\n';

                    ++reportRows;
                }
            }

            std::cout << caseId << ": parity retained\n";
        }

        std::string trailing;
        if (manifest >> trailing) {
            throw std::runtime_error(
                "Unexpected manifest content"
            );
        }

        csv.flush();

        if (!csv) {
            throw std::runtime_error("Report write failed");
        }

        std::cout
            << "Complete: " << reportRows << " stage rows\n"
            << "OpenCV: " << CV_VERSION << '\n'
            << "OpenCV thread setting: "
            << cv::getNumThreads() << '\n';

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Stage profiling failed: "
                  << error.what() << '\n';
        std::cerr << "Discard any incomplete report.\n";
        return 1;
    }
}