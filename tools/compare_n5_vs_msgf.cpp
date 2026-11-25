#include "application/application_controller.h"
#include "core/enhancement/msgf_thermal_enhancer.h"
#include "core/enhancement/n5_thermal_enhancer.h"
#include "core/enhancement/thermal_input.h"
#include "core/enhancement/thermal_quality_metrics.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using qart::application::ApplicationController;
using qart::application::ThermalProcessingMode;
using qart::core::enhancement::computePairQualityMetrics;
using qart::core::enhancement::computePlaneQualityStats;
using qart::core::enhancement::prepareThermalUint8Plane;

struct MethodAccumulator {
    long count = 0;
    double sumTimeMs = 0.0;
    double sumMean = 0.0;
    double sumStd = 0.0;
    double sumIe = 0.0;
    double sumAg = 0.0;
    double sumPiqe = 0.0;
    double sumPsnr = 0.0;
    long psnrFiniteCount = 0;
    double sumMae = 0.0;
    double sumMaxDiff = 0.0;
    double sumChangedPct = 0.0;

    void addInput(const qart::core::enhancement::PlaneQualityStats& s)
    {
        if (!s.valid) return;
        count++;
        sumMean += s.mean;
        sumStd += s.stdDev;
        sumIe += s.informationEntropy;
        sumAg += s.averageGradient;
        sumPiqe += s.piqe;
    }

    void addPair(double elapsedMs, const qart::core::enhancement::PairQualityMetrics& m)
    {
        if (!m.valid) return;
        count++;
        sumTimeMs += elapsedMs;
        sumMean += m.output.mean;
        sumStd += m.output.stdDev;
        sumIe += m.output.informationEntropy;
        sumAg += m.output.averageGradient;
        sumPiqe += m.output.piqe;
        if (std::isfinite(m.psnr)) {
            sumPsnr += m.psnr;
            psnrFiniteCount++;
        }
        sumMae += m.mae;
        sumMaxDiff += static_cast<double>(m.maxAbsDiff);
        sumChangedPct += m.changedPixelPercent;
    }

    void merge(const MethodAccumulator& o)
    {
        count += o.count;
        sumTimeMs += o.sumTimeMs;
        sumMean += o.sumMean;
        sumStd += o.sumStd;
        sumIe += o.sumIe;
        sumAg += o.sumAg;
        sumPiqe += o.sumPiqe;
        sumPsnr += o.sumPsnr;
        psnrFiniteCount += o.psnrFiniteCount;
        sumMae += o.sumMae;
        sumMaxDiff += o.sumMaxDiff;
        sumChangedPct += o.sumChangedPct;
    }
};

struct DatasetPaths {
    std::string label;
    std::vector<std::filesystem::path> files;
};

void collectDatasetPaths(
    const std::filesystem::path& datasetRoot,
    std::size_t maxPerDataset,
    std::vector<DatasetPaths>& out)
{
    for (const std::string& dsId : {std::string("roadscene"), std::string("msrs"), std::string("llvip")}) {
        ApplicationController controller;
        if (!controller.loadDataset(datasetRoot, dsId)) {
            continue;
        }
        std::vector<std::filesystem::path> files;
        const auto& pairs = controller.discoveredPairs();
        files.reserve(pairs.size());
        for (const auto& pair : pairs) {
            const auto p = datasetRoot / pair.thermalRelativePath();
            std::error_code ec;
            if (std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec)) {
                files.push_back(p);
            }
        }
        std::sort(files.begin(), files.end());
        if (maxPerDataset > 0 && files.size() > maxPerDataset) {
            // Evenly spaced stride sample so train/test and timeline are represented.
            std::vector<std::filesystem::path> sampled;
            sampled.reserve(maxPerDataset);
            const double step = static_cast<double>(files.size()) / static_cast<double>(maxPerDataset);
            for (std::size_t i = 0; i < maxPerDataset; ++i) {
                const auto idx = std::min<std::size_t>(
                    files.size() - 1,
                    static_cast<std::size_t>(std::floor(i * step))
                );
                sampled.push_back(files[idx]);
            }
            files = std::move(sampled);
        }
        if (!files.empty()) {
            out.push_back({dsId + " (" + std::to_string(files.size()) + "/" + std::to_string(pairs.size()) + ")",
                           std::move(files)});
        }
    }

    // Also evaluate FLIR validation thermal images if present on disk.
    const auto flirDir = datasetRoot / "raw" / "flir" / "images_thermal_val" / "data";
    std::error_code ec;
    if (std::filesystem::exists(flirDir, ec) && std::filesystem::is_directory(flirDir, ec)) {
        std::vector<std::filesystem::path> flirFiles;
        for (const auto& entry : std::filesystem::directory_iterator(flirDir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tif") {
                flirFiles.push_back(entry.path());
            }
        }
        std::sort(flirFiles.begin(), flirFiles.end());
        const auto totalFlir = flirFiles.size();
        if (maxPerDataset > 0 && flirFiles.size() > maxPerDataset) {
            std::vector<std::filesystem::path> sampled;
            sampled.reserve(maxPerDataset);
            const double step = static_cast<double>(flirFiles.size()) / static_cast<double>(maxPerDataset);
            for (std::size_t i = 0; i < maxPerDataset; ++i) {
                const auto idx = std::min<std::size_t>(
                    flirFiles.size() - 1,
                    static_cast<std::size_t>(std::floor(i * step))
                );
                sampled.push_back(flirFiles[idx]);
            }
            flirFiles = std::move(sampled);
        }
        if (!flirFiles.empty()) {
            out.push_back({"flir_val (" + std::to_string(flirFiles.size()) + "/" + std::to_string(totalFlir) + ")",
                           std::move(flirFiles)});
        }
    }
}

void printSummaryRow(
    const std::string& dataset,
    const std::string& method,
    const MethodAccumulator& a,
    bool isInput)
{
    if (a.count == 0) return;
    const double n = static_cast<double>(a.count);
    std::cout << std::left << std::setw(22) << dataset
              << std::setw(8) << method
              << std::right << std::setw(6) << a.count
              << std::fixed << std::setprecision(2)
              << std::setw(9) << (isInput ? 0.0 : a.sumTimeMs / n)
              << std::setw(8) << (a.sumMean / n)
              << std::setw(8) << (a.sumStd / n)
              << std::setprecision(3)
              << std::setw(8) << (a.sumIe / n)
              << std::setprecision(2)
              << std::setw(8) << (a.sumAg / n)
              << std::setw(8) << (a.sumPiqe / n);
    if (isInput) {
        std::cout << std::setw(10) << "-"
                  << std::setw(8) << "-"
                  << std::setw(8) << "-"
                  << std::setw(9) << "-";
    } else {
        const double meanPsnr = (a.psnrFiniteCount > 0)
            ? (a.sumPsnr / static_cast<double>(a.psnrFiniteCount))
            : 0.0;
        std::cout << std::setw(10) << meanPsnr
                  << std::setw(8) << (a.sumMae / n)
                  << std::setw(8) << (a.sumMaxDiff / n)
                  << std::setw(8) << (a.sumChangedPct / n) << "%";
    }
    std::cout << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    std::filesystem::path datasetRoot = "/Users/soncris/QART_Datasets";
    std::size_t maxPerDataset = 150;
    std::filesystem::path csvOut;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            datasetRoot = argv[++i];
        } else if (arg == "--limit" && i + 1 < argc) {
            maxPerDataset = static_cast<std::size_t>(std::max(0, std::atoi(argv[++i])));
        } else if (arg == "--csv" && i + 1 < argc) {
            csvOut = argv[++i];
        }
    }

    std::vector<DatasetPaths> datasets;
    collectDatasetPaths(datasetRoot, maxPerDataset, datasets);
    if (datasets.empty()) {
        std::cerr << "No datasets found under: " << datasetRoot << '\n';
        return 1;
    }

    std::ofstream csv;
    if (!csvOut.empty()) {
        csv.open(csvOut);
        csv << "dataset,file,method,time_ms,mean,std,ie,ag,piqe,psnr,mae,max_delta,changed_pct\n";
    }

    ApplicationController controller;
    MethodAccumulator totalInput;
    MethodAccumulator totalN5;
    MethodAccumulator totalMsgf;

    std::cout << std::left << std::setw(22) << "Dataset"
              << std::setw(8) << "Method"
              << std::right << std::setw(6) << "N"
              << std::setw(9) << "Time(ms)"
              << std::setw(8) << "Mean"
              << std::setw(8) << "Std"
              << std::setw(8) << "IE"
              << std::setw(8) << "AG"
              << std::setw(8) << "PIQE"
              << std::setw(10) << "PSNR(dB)"
              << std::setw(8) << "MAE"
              << std::setw(8) << "MaxD"
              << std::setw(9) << "Chg%"
              << '\n';
    std::cout << std::string(118, '-') << '\n';

    for (const auto& ds : datasets) {
        MethodAccumulator accInput;
        MethodAccumulator accN5;
        MethodAccumulator accMsgf;

        for (const auto& imgPath : ds.files) {
            cv::Mat raw = cv::imread(imgPath.string(), cv::IMREAD_UNCHANGED);
            if (raw.empty() || raw.depth() != CV_8U
                || (raw.channels() != 1 && raw.channels() != 3 && raw.channels() != 4)) {
                continue;
            }
            const cv::Mat prepared = prepareThermalUint8Plane(raw);
            const auto inStats = computePlaneQualityStats(prepared);
            accInput.addInput(inStats);

            const auto t0 = std::chrono::steady_clock::now();
            const auto n5Res = controller.processThermal(prepared, ThermalProcessingMode::N5PostHalf);
            const auto t1 = std::chrono::steady_clock::now();
            const double n5Ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const auto n5Metrics = computePairQualityMetrics(prepared, n5Res.outputImage);
            accN5.addPair(n5Ms, n5Metrics);

            const auto t2 = std::chrono::steady_clock::now();
            const auto msgfRes = controller.processThermal(prepared, ThermalProcessingMode::MsgfExperimental);
            const auto t3 = std::chrono::steady_clock::now();
            const double msgfMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
            const auto msgfMetrics = computePairQualityMetrics(prepared, msgfRes.outputImage);
            accMsgf.addPair(msgfMs, msgfMetrics);

            if (csv.is_open()) {
                const std::string fname = imgPath.filename().string();
                csv << ds.label << ',' << fname << ",Input,0,"
                    << inStats.mean << ',' << inStats.stdDev << ','
                    << inStats.informationEntropy << ',' << inStats.averageGradient << ','
                    << inStats.piqe << ",,,,\n";
                csv << ds.label << ',' << fname << ",N5," << n5Ms << ','
                    << n5Metrics.output.mean << ',' << n5Metrics.output.stdDev << ','
                    << n5Metrics.output.informationEntropy << ',' << n5Metrics.output.averageGradient << ','
                    << n5Metrics.output.piqe << ',' << n5Metrics.psnr << ','
                    << n5Metrics.mae << ',' << n5Metrics.maxAbsDiff << ','
                    << n5Metrics.changedPixelPercent << '\n';
                csv << ds.label << ',' << fname << ",MSGF," << msgfMs << ','
                    << msgfMetrics.output.mean << ',' << msgfMetrics.output.stdDev << ','
                    << msgfMetrics.output.informationEntropy << ',' << msgfMetrics.output.averageGradient << ','
                    << msgfMetrics.output.piqe << ',' << msgfMetrics.psnr << ','
                    << msgfMetrics.mae << ',' << msgfMetrics.maxAbsDiff << ','
                    << msgfMetrics.changedPixelPercent << '\n';
            }
        }

        printSummaryRow(ds.label, "Input", accInput, true);
        printSummaryRow(ds.label, "N5", accN5, false);
        printSummaryRow(ds.label, "MSGF", accMsgf, false);
        std::cout << '\n';

        totalInput.merge(accInput);
        totalN5.merge(accN5);
        totalMsgf.merge(accMsgf);
    }

    std::cout << std::string(118, '=') << '\n';
    printSummaryRow("ALL COMBINED", "Input", totalInput, true);
    printSummaryRow("ALL COMBINED", "N5", totalN5, false);
    printSummaryRow("ALL COMBINED", "MSGF", totalMsgf, false);

    return 0;
}
