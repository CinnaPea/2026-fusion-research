//
// Created by hakgu on 10/17/2026.
//

// Justification & Paper Alignment
// Engineering Decision — not paper-derived.
// Measure existing controller processing without altering scientific logic.
// Fixture loading, warm-ups and result reporting are outside timed intervals.

#include "application/application_controller.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Mode = qart::application::ThermalProcessingMode;

constexpr int Warmups = 3;
constexpr int Repeats = 10;

cv::Mat readPlane(
    const std::filesystem::path& path,
    int rows,
    int columns)
{
    cv::Mat image(rows, columns, CV_8UC1);
    const auto count =
        static_cast<std::streamsize>(image.total());

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw std::runtime_error(
            "Cannot open input: " + path.string()
        );
    }

    file.read(reinterpret_cast<char*>(image.data), count);

    if (
        file.gcount() != count
        || file.peek() != std::char_traits<char>::eof()
    ) {
        throw std::runtime_error(
            "Incorrect input size: " + path.string()
        );
    }

    return image;
}

// Linear interpolation between ordered samples.
// Reporting convention only; not a scientific quality metric.
double percentile(std::vector<double> values, double fraction)
{
    std::sort(values.begin(), values.end());

    const double position =
        fraction * static_cast<double>(values.size() - 1);

    const auto lower = static_cast<std::size_t>(position);
    const auto upper = std::min(lower + 1, values.size() - 1);
    const double weight = position - static_cast<double>(lower);

    return values[lower]
        + weight * (values[upper] - values[lower]);
}

} // namespace

int main(int argc, char** argv)
{
    try {
#ifndef NDEBUG
        throw std::runtime_error(
            "Use a Release build for this profiler"
        );
#endif

        if (argc != 3) {
            throw std::runtime_error(
                "Usage: profile_phase2_thermal "
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
                "Expected the validated 40-case native fixture packet"
            );
        }

        std::ofstream csv(outputPath);

        if (!csv) {
            throw std::runtime_error("Cannot create report");
        }

        csv << std::fixed << std::setprecision(6);
        csv << "case_id,width,height,mode,warmups,repeats,"
               "median_ms,p95_ms,min_ms,max_ms\n";

        qart::application::ApplicationController controller;
        std::unordered_set<std::string> seen;
        std::uint64_t checksum = 0;

        const std::array<Mode, 2> modes{
            Mode::Bypass,
            Mode::N5PostHalf
        };

        for (int index = 0; index < caseCount; ++index) {
            std::string caseId;
            int rows = 0;
            int columns = 0;
            double referenceEpsilon = 0.0;
            int constantFlag = 0;

            if (
                !(manifest >> caseId >> rows >> columns
                    >> referenceEpsilon >> constantFlag)
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
                || (constantFlag != 0 && constantFlag != 1)
            ) {
                throw std::runtime_error("Invalid manifest entry");
            }

            // Read once, before timing. The input is a decoded uint8 plane.
            const cv::Mat source = readPlane(
                directory / (caseId + "_input.bin"),
                rows, columns
            );

            for (int warmup = 0; warmup < Warmups; ++warmup) {
                for (const Mode mode : modes) {
                    const auto result =
                        controller.processThermal(source, mode);

                    checksum += result.outputImage
                        .at<unsigned char>(0, 0);
                }
            }

            std::array<std::vector<double>, 2> samples;
            for (auto& values : samples) {
                values.reserve(Repeats);
            }

            for (int repeat = 0; repeat < Repeats; ++repeat) {
                // Alternate order to reduce a fixed method-order bias.
                for (int slot = 0; slot < 2; ++slot) {
                    const int method = (repeat + slot) % 2;

                    const auto start = Clock::now();
                    const auto result =
                        controller.processThermal(
                            source, modes[method]
                        );
                    const auto end = Clock::now();

                    samples[method].push_back(
                        std::chrono::duration<double, std::milli>(
                            end - start
                        ).count()
                    );

                    // Consume results after timing.
                    checksum += result.outputImage
                        .at<unsigned char>(0, 0);
                }
            }

            for (int method = 0; method < 2; ++method) {
                const auto& values = samples[method];
                const auto bounds =
                    std::minmax_element(values.begin(), values.end());

                csv << caseId << ','
                    << columns << ',' << rows << ','
                    << (method == 0 ? "bypass" : "n5") << ','
                    << Warmups << ',' << Repeats << ','
                    << percentile(values, 0.50) << ','
                    << percentile(values, 0.95) << ','
                    << *bounds.first << ','
                    << *bounds.second << '\n';
            }

            std::cout << caseId << ": profiled\n";
        }

        std::string trailing;
        if (manifest >> trailing) {
            throw std::runtime_error("Unexpected manifest content");
        }

        csv.flush();
        if (!csv) {
            throw std::runtime_error("Report write failed");
        }

        std::cout
            << "Complete: 80 report rows\n"
            << "OpenCV: " << CV_VERSION << '\n'
            << "OpenCV thread setting: " << cv::getNumThreads()
            << '\n'
            << "Checksum: " << checksum << '\n';

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Profiling failed: " << error.what() << '\n';
        std::cerr
            << "Do not use a partial CSV as a completed report.\n";
        return 1;
    }
}