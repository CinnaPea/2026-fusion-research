// Justification & Paper Alignment
// Choice: check conversion, invalid input and storage/ROI contracts directly.
// Classification: Engineering Decision — not paper-derived.
// Rule: test source immutability and owned results; no quality assertions.
#include "core/enhancement/thermal_input.h"
#include <opencv2/core.hpp>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
template<class F> void rejects(F operation) {
    try { operation(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid thermal input accepted");
}
void checkOwned(const cv::Mat& source, const cv::Mat& before, cv::Mat output) {
    require(output.type() == CV_8UC1 && output.size() == source.size(), "output geometry/type");
    require(output.data != source.data, "output aliases source");
    require(cv::norm(source, before, cv::NORM_INF) == 0, "source modified");
    output.setTo(17);
    require(cv::norm(source, before, cv::NORM_INF) == 0, "output mutation reaches source");
}
}
int main() {
    using namespace qart::core::enhancement;
    try {
        // Native strided submatrices must not depend on pixels outside their ROI.
        for (int channels : {1,3,4}) {
            cv::Mat parent(9,13,CV_MAKETYPE(CV_8U,channels));
            cv::RNG rng(42); rng.fill(parent,cv::RNG::UNIFORM,0,256);
            cv::Mat roi=parent(cv::Rect(2,1,7,5));
            require(!roi.isContinuous(), "fixture should be strided");
            cv::Mat before=roi.clone();
            cv::Mat prepared=prepareThermalUint8Plane(roi);
            require(cv::norm(prepared,prepareThermalUint8Plane(before),cv::NORM_INF)==0,"ROI conversion differs");
            if (channels==1) require(cv::norm(prepared,before,cv::NORM_INF)==0,"grayscale values changed");
            checkOwned(roi,before,prepared);
        }
        for (int level : {0,1,127,255}) {
            cv::Mat plane(1,1,CV_8UC1,cv::Scalar(level));
            auto output=bypassThermal(plane);
            require(output.at<unsigned char>(0,0)==level,"BYPASS changed constant");
            checkOwned(plane,plane.clone(),output);
        }
        cv::Mat bgr(1,3,CV_8UC3);
        bgr.at<cv::Vec3b>(0,0)={255,0,0};bgr.at<cv::Vec3b>(0,1)={0,255,0};bgr.at<cv::Vec3b>(0,2)={0,0,255};
        auto gray=prepareThermalUint8Plane(bgr);
        require(gray.at<unsigned char>(0,0)==29 && gray.at<unsigned char>(0,1)==150 && gray.at<unsigned char>(0,2)==76,"BGR order mismatch");
        cv::Mat bgra(1,2,CV_8UC4);bgra.at<cv::Vec4b>(0,0)={12,34,56,0};bgra.at<cv::Vec4b>(0,1)={12,34,56,255};
        auto alpha=prepareThermalUint8Plane(bgra);
        require(alpha.at<unsigned char>(0,0)==alpha.at<unsigned char>(0,1),"alpha affects intensity");
        cv::Mat empty, wrongDepth(2,3,CV_16UC1), wrongChannels(2,3,CV_8UC2);
        int sizes[]={2,3,4};cv::Mat volume(3,sizes,CV_8UC1);
        for (const auto& invalid : {empty,wrongDepth,wrongChannels,volume}) {
            rejects([&]{(void)prepareThermalUint8Plane(invalid);});
            rejects([&]{(void)bypassThermal(invalid);});
        }
        rejects([&]{(void)bypassThermal(bgr);});
        rejects([&]{(void)bypassThermal(bgra);});
        std::cout<<"Thermal input/BYPASS contracts passed\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
