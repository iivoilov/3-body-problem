#include "alien_world.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

using namespace orbit;
namespace {
constexpr double duration = 36;
constexpr unsigned intervalSteps = 32;
constexpr unsigned frameCount = static_cast<unsigned>(duration / (alienStep * intervalSteps)) + 1;
void writeFloat(std::ostream& file, double number) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    const float value = static_cast<float>(number);
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    const char bytes[]{static_cast<char>(bits), static_cast<char>(bits >> 8),
                       static_cast<char>(bits >> 16), static_cast<char>(bits >> 24)};
    file.write(bytes, sizeof(bytes));
}
void vectorJson(std::ostream& file, Vec3d v) { file << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: orbit02_export OUTPUT_DIRECTORY");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        std::ofstream binary(directory / "world.bin", std::ios::binary);
        std::ofstream manifest(directory / "manifest.json");
        if (!binary || !manifest) throw std::runtime_error("Cannot open Orbit / 02 output files.");
        AlienSimulation simulation;
        manifest << std::setprecision(17)
                 << "{\n  \"format\":\"orbit-02-trajectory\", \"version\":1, \"encoding\":\"float32-le\",\n"
                 << "  \"file\":\"world.bin\", \"fieldsPerFrame\":20, \"frameCount\":" << frameCount << ",\n"
                 << "  \"duration\":" << duration << ", \"sampleInterval\":" << alienStep*intervalSteps << ",\n"
                 << "  \"physicsStep\":" << alienStep << ", \"playbackScale\":" << alienPlayback << ",\n"
                 << "  \"G\":" << stellarGravity << ", \"rotationPeriod\":" << alienRotationPeriod << ",\n"
                 << "  \"units\":{\"length\":\"AU\",\"mass\":\"solar mass\",\"time\":\"Julian year\",\"flux\":\"Earth incident flux\"},\n"
                 << "  \"description\":\"Invented compact triple with measured Alpha Centauri mass ratios and a massless planet. Not a reconstruction of Alpha Centauri or canonical fictional orbits. Disks and planet enlarged for visibility.\",\n"
                 << "  \"fields\":[\"alpha.x\",\"alpha.y\",\"alpha.z\",\"beta.x\",\"beta.y\",\"beta.z\",\"proxima.x\",\"proxima.y\",\"proxima.z\",\"planet.x\",\"planet.y\",\"planet.z\",\"flux\",\"nearestSun\",\"closestStellarPair\",\"stellarEnergyError\",\"stellarAngularError\",\"alphaFlux\",\"betaFlux\",\"proximaFlux\"],\n"
                 << "  \"sources\":[\"https://doi.org/10.3847/1538-3881/abfaff\",\"https://www.eso.org/public/archives/releases/sciencepapers/eso2202/eso2202a.pdf\"],\n"
                 << "  \"stars\":[\n";
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& star = simulation.stars[i];
            manifest << "    {\"name\":\"" << stellarNames[i] << "\",\"mass\":" << star.mass
                     << ",\"radius\":" << stellarRadii[i] << ",\"displayRadius\":" << stellarDisplayRadii[i]
                     << ",\"luminosity\":" << stellarLuminosities[i] << ",\"color\":["
                     << stellarColors[i].x << ',' << stellarColors[i].y << ',' << stellarColors[i].z << "],\"position\":";
            vectorJson(manifest, star.position);
            manifest << ",\"velocity\":"; vectorJson(manifest, star.velocity);
            manifest << '}' << (i == 2 ? "\n" : ",\n");
        }
        manifest << "  ],\n  \"planet\":{\"name\":\"Trisolaris\",\"mass\":0,\"testParticle\":true,\"radius\":"
                 << alienPlanetRadius << ",\"displayRadius\":0.13,\"position\":";
        vectorJson(manifest, simulation.planet.position);
        manifest << ",\"velocity\":"; vectorJson(manifest, simulation.planet.velocity);
        manifest << "}\n}\n";
        for (unsigned frame = 0; frame < frameCount; ++frame) {
            for (std::size_t body = 0; body < 4; ++body) {
                const auto p = body < 3 ? simulation.stars[body].position : simulation.planet.position;
                writeFloat(binary, p.x); writeFloat(binary, p.y); writeFloat(binary, p.z);
            }
            const auto sample = simulation.measure();
            for (double value : {sample.flux, sample.nearest, sample.separation, sample.energyError, sample.angularError})
                writeFloat(binary, value);
            for (double value : sample.contributions) writeFloat(binary, value);
            if (frame+1 < frameCount)
                for (unsigned step = 0; step < intervalSteps; ++step)
                    if (!simulation.step()) throw std::runtime_error("Surface contact during Orbit / 02 export.");
        }
        if (!binary || !manifest) throw std::runtime_error("Failed writing Orbit / 02 data.");
        std::cout << "Orbit / 02: " << frameCount << " frames, " << duration << " years, " << frameCount*80 << " bytes.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
