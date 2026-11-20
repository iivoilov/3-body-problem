#pragma once
#include <string>
namespace orbit {
struct AlienViewerOptions {
    bool smoke = false, surface = true;
    double startTime = 0, speed = 1;
    std::string capture, title, preset="02-01";
};
int runAlienViewer(const AlienViewerOptions& options);
}
