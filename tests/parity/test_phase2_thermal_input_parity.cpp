// Justification & Paper Alignment
// Choice: compare exact bytes against unchanged Python reference outputs.
// Classification: Engineering Decision — not paper-derived.
// Rule: read deterministic lossless fixtures; differences fail the test.
// This validates implementation parity, not scientific image quality.
#include "core/enhancement/thermal_input.h"
#include <opencv2/core.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
std::vector<unsigned char> bytes(const std::filesystem::path& p,std::size_t count) {
    std::ifstream file(p,std::ios::binary);
    if (!file) throw std::runtime_error("missing fixture: "+p.string());
    std::vector<unsigned char> result(count);
    file.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(count));
    if (static_cast<std::size_t>(file.gcount())!=count || file.peek()!=std::ifstream::traits_type::eof())
        throw std::runtime_error("fixture size mismatch: "+p.string());
    return result;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("usage: parity FIXTURE_DIRECTORY");
        std::filesystem::path root(argv[1]);std::ifstream manifest(root/"cases.txt");
        if(!manifest)throw std::runtime_error("missing manifest");
        std::string name;int rows=0,cols=0,channels=0,count=0;
        while(manifest>>name>>rows>>cols>>channels) {
            if(rows<=0||cols<=0||(channels!=1&&channels!=3&&channels!=4))throw std::runtime_error("invalid manifest geometry");
            cv::Mat input(rows,cols,CV_MAKETYPE(CV_8U,channels));
            auto raw=bytes(root/(name+"_input.bin"),input.total()*input.elemSize());
            std::copy(raw.begin(),raw.end(),input.data);
            auto expected=bytes(root/(name+"_expected.bin"),input.total());
            auto prepared=qart::core::enhancement::prepareThermalUint8Plane(input);
            auto bypass=qart::core::enhancement::bypassThermal(prepared);
            for(std::size_t i=0;i<expected.size();++i)
                if(prepared.data[i]!=expected[i]||bypass.data[i]!=expected[i])throw std::runtime_error("pixel mismatch: "+name);
            ++count;
        }
        if(!manifest.eof()||count!=12)throw std::runtime_error("manifest incomplete or incorrect case count");
        std::cout<<count<<" Python/C++ preparation+BYPASS fixtures exact; OpenCV "<<CV_VERSION<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
