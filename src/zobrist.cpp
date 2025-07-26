#include "zobrist.h"
#include <cstdint>
#include <random>

struct ZobristInitializer {
  ZobristInitializer() {
    std::random_device device;
    std::seed_seq seed{device(), device(), device(), device()};
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<std::uint64_t> distribution;

    for (auto &color : ZOBRIST_PIECE) {
      for (auto &pieceType : color) {
        for (std::uint64_t &zobrist : pieceType) {
          zobrist = distribution(gen);
        }
      }
    }

    ZOBRIST_TURN = distribution(gen);

    for (std::uint64_t &zobrist : ZOBRIST_CASTLING_RIGHTS) {
      zobrist = distribution(gen);
    }

    for (std::uint64_t &zobrist : ZOBRIST_EN_PASSANT_FILE) {
      zobrist = distribution(gen);
    }
  }
};

static ZobristInitializer zobristInitializerInstance;
