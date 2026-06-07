/**
 * @file replay_main.cpp
 * @brief ITCH Replay Demo — generates synthetic data and replays through LOB
 *
 * Usage:
 *   arctic_replay                          # Generate 100k msgs + replay
 *   arctic_replay <file.itch>              # Replay existing ITCH file
 *   arctic_replay --generate <N> <file>    # Generate N msgs to file
 */

#include "replay_engine.hpp"
#include "tsc_clock.hpp"
#include <iostream>
#include <string>
#include <memory>

int main(int argc, char* argv[]) {
    std::cout << R"(
    ╔═══════════════════════════════════════════════════════════╗
    ║  ARCTIC — ITCH 5.0 Replay Engine                        ║
    ║  Zero-copy mmap → binary parse → LOB insert             ║
    ╚═══════════════════════════════════════════════════════════╝
)" << std::endl;

    std::string itch_path;
    int generate_count = 0;

    if (argc >= 4 && std::string(argv[1]) == "--generate") {
        // Generate mode
        generate_count = std::atoi(argv[2]);
        itch_path = argv[3];
        std::cout << "[Mode] Generating " << generate_count << " synthetic ITCH messages\n\n";
        if (!arctic::ITCHGenerator::generate(itch_path, generate_count, 100.0, 42)) {
            std::cerr << "Failed to generate ITCH data\n";
            return 1;
        }
        std::cout << "\n";
    } else if (argc >= 2) {
        // Replay existing file
        itch_path = argv[1];
    } else {
        // Default: generate + replay
        itch_path = "synthetic_itch.bin";
        generate_count = 100000;
        std::cout << "[Mode] Generating " << generate_count 
                  << " synthetic ITCH messages + replay\n\n";
        if (!arctic::ITCHGenerator::generate(itch_path, generate_count, 100.0, 42)) {
            std::cerr << "Failed to generate ITCH data\n";
            return 1;
        }
        std::cout << "\n";
    }

    // Create OrderBook centered at $100.00 with $0.01 ticks
    auto book = std::make_unique<arctic::OrderBook>(0.01, 100.0);

    std::cout << "[Replay] Loading " << itch_path << "...\n";
    auto stats = arctic::ReplayEngine::replay(itch_path, *book, 0);

    if (stats.parse_stats.messages_parsed == 0) {
        std::cerr << "\nNo messages parsed. Check file format.\n";
        return 1;
    }

    return 0;
}
