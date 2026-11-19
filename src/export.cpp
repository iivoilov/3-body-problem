#include "scene.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace orbit;
namespace {
constexpr double duration = 2048;
constexpr unsigned sampleSteps = 32;
constexpr unsigned frameCount = static_cast<unsigned>(duration / (sampleSteps * physicsStep)) + 1;
constexpr unsigned fieldsPerFrame = 14;

void writeFloat(std::ostream& file, float value) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "Export requires IEEE-754 float32.");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const char bytes[]{static_cast<char>(bits), static_cast<char>(bits >> 8),
                       static_cast<char>(bits >> 16), static_cast<char>(bits >> 24)};
    file.write(bytes, sizeof(bytes));
}

void writeFrame(std::ostream& file, const Simulation& simulation) {
    for (const auto& body : simulation.bodies) {
        writeFloat(file, static_cast<float>(body.position.x));
        writeFloat(file, static_cast<float>(body.position.y));
        writeFloat(file, static_cast<float>(body.position.z));
    }
    const auto& sample = simulation.diagnostics.history.back();
    const float missing = std::numeric_limits<float>::quiet_NaN();
    writeFloat(file, sample.ratioValid ? static_cast<float>(sample.periodRatio) : missing);
    for (const auto& elements : sample.elements) writeFloat(file, static_cast<float>(elements.eccentricity));
    for (int i = 0; i < 2; ++i) writeFloat(file, sample.phaseValid[i] ? static_cast<float>(sample.phaseTurns[i]) : missing);
}

void exportPreset(const std::filesystem::path& path, bool resonance) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write " + path.string());
    Simulation simulation{resonance};
    for (unsigned frame = 0; frame < frameCount; ++frame) {
        writeFrame(file, simulation);
        if (frame + 1 == frameCount) break;
        for (unsigned step = 0; step < sampleSteps; ++step)
            if (!simulation.step()) throw std::runtime_error("Surface contact during export of " + path.string());
    }
    if (!file) throw std::runtime_error("Failed writing " + path.string());
    std::cout << path.string() << ": " << frameCount << " frames, " << frameCount*fieldsPerFrame*4 << " bytes\n";
}

void exportManifest(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("Cannot write " + path.string());
    file << std::setprecision(17)
         << "{\n  \"format\":\"orbit-lab-trajectory\",\n  \"version\":1,\n"
         << "  \"encoding\":\"float32-le\",\n  \"fieldsPerFrame\":" << fieldsPerFrame << ",\n"
         << "  \"frameCount\":" << frameCount << ",\n  \"duration\":" << duration << ",\n"
         << "  \"sampleInterval\":" << sampleSteps*physicsStep << ",\n"
         << "  \"telemetryInterval\":" << OrbitDiagnostics::stride*physicsStep << ",\n"
         << "  \"physicsStep\":" << physicsStep << ",\n  \"gravity\":" << gravity << ",\n"
         << "  \"playbackScale\":" << playbackScale << ",\n  \"initialPeriodRatio\":" << initialPeriodRatio << ",\n"
         << "  \"coordinateSystem\":\"Barycentric; XZ orbital plane, Y up; prograde spin about -Y\",\n"
         << "  \"fields\":[\"sol.x\",\"sol.y\",\"sol.z\",\"azure.x\",\"azure.y\",\"azure.z\","
            "\"ember.x\",\"ember.y\",\"ember.z\",\"periodRatio\",\"eccentricity.azure\",\"eccentricity.ember\","
            "\"phaseTurns.azure\",\"phaseTurns.ember\"],\n  \"bodies\":[\n";
    for (int i = 0; i < 3; ++i) {
        const Vec3 color = i == 0 ? Vec3{1,0.7f,0.2f} : planets[i-1].color;
        file << "    {\"name\":\"" << (i == 0 ? "Sol" : planets[i-1].name) << "\",\"mass\":"
             << (i == 0 ? sunMass : planets[i-1].mass) << ",\"radius\":" << bodyRadii[i]
             << ",\"spinRate\":" << spinRates[i] << ",\"color\":[" << color.x << ',' << color.y << ',' << color.z << "]}"
             << (i == 2 ? "\n" : ",\n");
    }
    file << "  ],\n  \"presets\":[\n"
            "    {\"id\":\"circular\",\"label\":\"Circular start\",\"file\":\"circular.bin\","
            "\"description\":\"Near-circular initial orbits. Automatic capture or damping to a fixed lock is not established.\"},\n"
            "    {\"id\":\"resonance\",\"label\":\"Resonance candidate\",\"file\":\"resonance.bin\","
            "\"description\":\"Chosen eccentricities and phases give bounded critical angles in a finite test; this is not spontaneous capture or proof of permanent resonance.\"}\n"
            "  ],\n  \"missingTelemetry\":\"IEEE-754 NaN marks undefined period ratios or unreliable periapsis angles.\"\n}\n";
    if (!file) throw std::runtime_error("Failed writing " + path.string());
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: orbit_export OUTPUT_DIRECTORY\nExports both 2048-second presets; no graphics display is needed.\n";
            return EXIT_FAILURE;
        }
        const std::filesystem::path directory = argv[1];
        std::filesystem::create_directories(directory);
        exportPreset(directory / "circular.bin", false);
        exportPreset(directory / "resonance.bin", true);
        exportManifest(directory / "manifest.json");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Export error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
