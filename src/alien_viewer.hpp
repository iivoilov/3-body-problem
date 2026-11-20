#pragma once
#include <string>
namespace orbit {
struct AlienViewerOptions {
    bool smoke = false, surface = true;
    double startTime = 0, speed = 1;
    std::string capture, title;
};
int runAlienViewer(const AlienViewerOptions& options);
}
