// Gold integration test - Full pipeline matching pipe/gold.cpp
// With stage dumps for comparison against pipe reference

#include "pqtr.hpp"
#include <iostream>
#include <cstring>

namespace pqtr::Labs {
    std::unique_ptr<Head> sonyHead();
    std::unique_ptr<Step> rawprepareStep();
    std::unique_ptr<Step> temperatureStep();
    std::unique_ptr<Step> highlightsStep();
    std::unique_ptr<Step> demosaicStep();
    std::unique_ptr<Step> exposureStep();
    std::unique_ptr<Step> colorinStep();
    std::unique_ptr<Step> channelmixerStep();
    std::unique_ptr<Step> colorbalanceStep();
    std::unique_ptr<Step> filmicStep();
    std::unique_ptr<Step> bilatStep();
    std::unique_ptr<Step> coloroutStep();
    std::unique_ptr<Step> dumpStep(const std::string &path, int channels = 4);
    std::unique_ptr<Tail> jsonTail(const std::string &path);
    std::unique_ptr<Tail> pngTail(const std::string &path);
}

using namespace pqtr::Labs;

int main() {
    // Create output directory
    system("mkdir -p tmp/var/labs");

    auto pipe = pqtr::pipe();

    // Head: Sony ARW decoder
    pipe->head(sonyHead())

        // Sensor stage (bayer, 1 channel)
        .body("rawprepare", rawprepareStep())
        .body("dump_01", dumpStep("tmp/var/labs/01_rawprepare.bin", 1))

        .body("temperature", temperatureStep())
        .body("dump_02", dumpStep("tmp/var/labs/02_temperature.bin", 1))

        .body("highlights", highlightsStep())
        .body("dump_03", dumpStep("tmp/var/labs/03_highlights.bin", 1))

        // Camera stage (RGBA, 4 channels)
        .body("demosaic", demosaicStep())
        .body("dump_04", dumpStep("tmp/var/labs/04_demosaic.bin", 4))

        .body("exposure", exposureStep())
        .body("dump_05", dumpStep("tmp/var/labs/05_exposure.bin", 4))

        // Scene stage (Rec2020 RGBA, 4 channels)
        .body("colorin", colorinStep())
        .body("dump_06", dumpStep("tmp/var/labs/06_colorin.bin", 4))

        .body("channelmixer", channelmixerStep())
        .body("dump_07", dumpStep("tmp/var/labs/07_channelmixer.bin", 4))

        .body("colorbalance", colorbalanceStep())
        .body("dump_08", dumpStep("tmp/var/labs/08_colorbalance.bin", 4))

        .body("filmic", filmicStep())
        .body("dump_09", dumpStep("tmp/var/labs/09_filmic.bin", 4))

        .body("bilat", bilatStep())
        .body("dump_10", dumpStep("tmp/var/labs/10_bilat.bin", 4))

        .body("colorout", coloroutStep())
        .body("dump_11", dumpStep("tmp/var/labs/11_colorout.bin", 4))

        // Tails
        .tail(jsonTail("tmp/var/labs/gold.json"))
        .tail(pngTail("tmp/var/labs/gold.png"));

    // Pass ARW filename to pump
    const char* arw = "src/test/raws/sony.ARW";
    pipe->pump((void*)arw, strlen(arw));

    return 0;
}
