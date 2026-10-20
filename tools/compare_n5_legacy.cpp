//
// Created by hakgu on 10/17/2026.
//

#include "core/enhancement/n5_thermal_enhancer.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

cv::Mat readPlane(const fs::path& path)
{
    cv::Mat image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        throw std::runtime_error("Cannot decode: " + path.string());
    }
    if (image.dims != 2 || image.type() != CV_8UC1) {
        throw std::runtime_error(
            "Expected unchanged uint8 grayscale PNG: " + path.string());
    }
    return image;
}

void savePlane(const fs::path& path, const cv::Mat& image)
{
    if (!cv::imwrite(path.string(), image)) {
        throw std::runtime_error("Cannot write: " + path.string());
    }

    const cv::Mat decoded = readPlane(path);
    if (decoded.size() != image.size()
        || cv::norm(decoded, image, cv::NORM_INF) != 0.0) {
        throw std::runtime_error("PNG round-trip mismatch: " + path.string());
    }
}

void writeMetrics(
    std::ostream& csv, int caseId,
    const char* mode, const cv::Mat& image)
{
    // Same histogram definition as legacy triage::entropy().
    const int channels[] = {0};
    const int sizes[] = {256};
    const float range[] = {0.0f, 256.0f};
    const float* ranges[] = {range};

    cv::Mat histogram;
    cv::calcHist(
        &image, 1, channels, cv::Mat(),
        histogram, 1, sizes, ranges, true, false);

    const double histogramSum = cv::sum(histogram)[0] + 1e-12;
    double entropy = 0.0;
    for (int bin = 0; bin < 256; ++bin) {
        const double probability =
            histogram.at<float>(bin) / histogramSum;
        if (probability > 0.0) {
            entropy -= probability * std::log2(probability);
        }
    }

    cv::Scalar mean, deviation;
    cv::meanStdDev(image, mean, deviation);
    const double rms = deviation[0];

    cv::Mat laplacian;
    cv::Laplacian(image, laplacian, CV_64F);
    cv::meanStdDev(laplacian, mean, deviation);
    const double laplacianVariance = deviation[0] * deviation[0];

    cv::Mat gx, gy;
    cv::Sobel(image, gx, CV_32F, 1, 0, 3);
    cv::Sobel(image, gy, CV_32F, 0, 1, 3);
    const cv::Mat squaredGradient = gx.mul(gx) + gy.mul(gy);
    const double sobelEnergy = cv::mean(squaredGradient)[0];

    const double pixels = static_cast<double>(image.total());

    csv << caseId << ',' << mode << ','
        << image.cols << ',' << image.rows << ','
        << entropy << ',' << rms << ','
        << laplacianVariance << ',' << sobelEnergy << ','
        << cv::countNonZero(image == 0) / pixels << ','
        << cv::countNonZero(image == 255) / pixels << '\n';

    if (!csv) {
        throw std::runtime_error("Cannot write metrics");
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr
            << "Usage: compare_n5_legacy LEGACY_RUN_DIR NEW_OUTPUT_DIR\n";
        return 2;
    }

    try {
        const fs::path sourceDirectory = argv[1];
        const fs::path outputDirectory = argv[2];

        // Validate both input pairs before creating an output directory.
        // std::array<cv::Mat, 2> inputs;
        // std::array<cv::Mat, 2> legacyOutputs;
        std::ifstream sourceMarker(sourceDirectory / "COMPLETE.txt");
        if (!sourceMarker) {
            throw std::runtime_error("Legacy run has no COMPLETE.txt");
        }

        std::string markerLine;
        int caseCount = 0;
        bool foundCaseCount = false;

        while (std::getline(sourceMarker, markerLine)) {
            if (!markerLine.empty() && markerLine.back() == '\r') {
                markerLine.pop_back();
            }

            if (markerLine.rfind("cases=", 0) == 0) {
                if (foundCaseCount) {
                    throw std::runtime_error("Duplicate legacy case count");
                }

                const std::string value = markerLine.substr(6);
                std::size_t consumed = 0;
                caseCount = std::stoi(value, &consumed);

                if (consumed != value.size()) {
                    throw std::runtime_error("Invalid legacy case count");
                }
                foundCaseCount = true;
            }
        }

        if (!foundCaseCount || caseCount <= 0 || caseCount > 1000) {
            throw std::runtime_error("Invalid or missing legacy case count");
        }

        std::vector<cv::Mat> inputs(static_cast<std::size_t>(caseCount));
        std::vector<cv::Mat> legacyOutputs(static_cast<std::size_t>(caseCount));

        for (std::size_t index = 0; index < inputs.size(); ++index) {
            const std::string id = std::to_string(index + 1);
            inputs[index] = readPlane(
                sourceDirectory / (id + "__input.png"));
            legacyOutputs[index] = readPlane(
                sourceDirectory / (id + "__legacy.png"));

            if (inputs[index].size() != legacyOutputs[index].size()) {
                throw std::runtime_error(
                    "Input/legacy geometry mismatch for case " + id);
            }
        }

        if (!fs::create_directory(outputDirectory)) {
            throw std::runtime_error(
                "Output directory exists; choose a new output directory");
        }

        std::ofstream csv(outputDirectory / "metrics.csv");
        if (!csv) {
            throw std::runtime_error("Cannot create metrics.csv");
        }

        csv << std::setprecision(17)
            << "case_number,mode,width,height,entropy,rms,"
               "laplacian_variance,sobel_energy,"
               "minimum_clip_fraction,maximum_clip_fraction\n";

        qart::core::enhancement::N5ThermalEnhancer enhancer;

        for (std::size_t index = 0; index < inputs.size(); ++index) {
            const int caseId = static_cast<int>(index + 1);
            const std::string id = std::to_string(caseId);
            const cv::Mat original = inputs[index].clone();

            // Calls the accepted implementation without changing parameters.
            const auto result = enhancer.enhance(inputs[index]);

            if (result.outputImage.type() != CV_8UC1
                || result.outputImage.size() != original.size()) {
                throw std::runtime_error("N5 output contract violation");
            }

            if (cv::norm(inputs[index], original, cv::NORM_INF) != 0.0) {
                throw std::runtime_error("N5 modified its source");
            }

            savePlane(outputDirectory / (id + "__bypass.png"), original);
            savePlane(outputDirectory / (id + "__n5.png"),
                      result.outputImage);
            savePlane(outputDirectory / (id + "__legacy.png"),
                      legacyOutputs[index]);

            writeMetrics(csv, caseId, "bypass", original);
            writeMetrics(csv, caseId, "n5_accepted", result.outputImage);
            writeMetrics(csv, caseId, "legacy_default", legacyOutputs[index]);

            std::cout << "Case " << caseId
                      << ": source unchanged; 3 PNG round-trips passed\n";
        }

        csv.close();
        if (!csv) {
            throw std::runtime_error("Cannot finalize metrics.csv");
        }

        std::ofstream marker(outputDirectory / "COMPLETE.txt");
        marker << "comparison=BYPASS,N5_accepted,legacy_default\n"
                << "cases=" << caseCount << '\n'
                << "rows=" << caseCount * 3 << '\n'
               << "legacy_run_directory="
               << fs::absolute(sourceDirectory).string() << '\n'
               << "n5=existing_linked_qart_core_implementation\n"
               << "opencv=" << CV_VERSION << '\n'
               << "additional_postprocessing=none\n";
        marker.close();

        if (!marker) {
            throw std::runtime_error("Cannot finalize completion marker");
        }

        std::cout << "Complete: " << caseCount
          << " cases; " << caseCount * 3
          << " metric rows\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Comparison failed: " << error.what()
                  << "\nTreat this run as incomplete.\n";
        return 1;
    }
}