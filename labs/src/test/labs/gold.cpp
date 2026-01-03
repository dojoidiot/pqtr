// Gold integration test - Full pipeline matching pipe/gold.cpp
// With stage dumps for comparison against pipe reference

#include "labs.hpp"
#include <iostream>
#include <cstring>

int main() {
    // Create output directory
    system("mkdir -p tmp/var/labs");

    auto pipe = pqtr::pipe();

    // Head: Sony ARW decoder
    pipe->head(std::make_unique<pqtr::SonyHead>())

        // Sensor stage (bayer, 1 channel)
        .body("rawprepare", std::make_unique<pqtr::RawprepareStep>())
        .body("dump_01", std::make_unique<pqtr::DumpStep>("tmp/var/labs/01_rawprepare.bin", 1))

        .body("temperature", std::make_unique<pqtr::TemperatureStep>())
        .body("dump_02", std::make_unique<pqtr::DumpStep>("tmp/var/labs/02_temperature.bin", 1))

        .body("highlights", std::make_unique<pqtr::HighlightsStep>())
        .body("dump_03", std::make_unique<pqtr::DumpStep>("tmp/var/labs/03_highlights.bin", 1))

        // Camera stage (RGBA, 4 channels)
        .body("demosaic", std::make_unique<pqtr::DemosaicStep>())
        .body("dump_04", std::make_unique<pqtr::DumpStep>("tmp/var/labs/04_demosaic.bin", 4))

        .body("exposure", std::make_unique<pqtr::ExposureStep>())
        .body("dump_05", std::make_unique<pqtr::DumpStep>("tmp/var/labs/05_exposure.bin", 4))

        // Scene stage (Rec2020 RGBA, 4 channels)
        .body("colorin", std::make_unique<pqtr::ColorinStep>())
        .body("dump_06", std::make_unique<pqtr::DumpStep>("tmp/var/labs/06_colorin.bin", 4))

        .body("channelmixer", std::make_unique<pqtr::ChannelMixerStep>())
        .body("dump_07", std::make_unique<pqtr::DumpStep>("tmp/var/labs/07_channelmixer.bin", 4))

        .body("colorbalance", std::make_unique<pqtr::ColorBalanceStep>())
        .body("dump_08", std::make_unique<pqtr::DumpStep>("tmp/var/labs/08_colorbalance.bin", 4))

        .body("filmic", std::make_unique<pqtr::FilmicStep>())
        .body("dump_09", std::make_unique<pqtr::DumpStep>("tmp/var/labs/09_filmic.bin", 4))

        .body("bilat", std::make_unique<pqtr::BilatStep>())
        .body("dump_10", std::make_unique<pqtr::DumpStep>("tmp/var/labs/10_bilat.bin", 4))

        .body("colorout", std::make_unique<pqtr::ColoroutStep>())
        .body("dump_11", std::make_unique<pqtr::DumpStep>("tmp/var/labs/11_colorout.bin", 4))

        // Tails
        .tail(std::make_unique<pqtr::JsonTail>("tmp/var/labs/gold.json"))
        .tail(std::make_unique<pqtr::PngTail>("tmp/var/labs/gold.png"));

    // Pass ARW filename to pump
    const char* arw = "src/test/raws/sony.ARW";
    pipe->pump((void*)arw, strlen(arw));

    return 0;
}
