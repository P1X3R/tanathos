#include "sysifus.h"
#include <array>
#include <random>

std::array<std::array<std::array<std::uint64_t, BOARD_AREA>, Piece::KING + 1>,
           2>
    ZOBRIST_PIECE;
std::uint64_t ZOBRIST_TURN;
std::array<std::uint64_t, 1 << 4> ZOBRIST_CASTLING_RIGHTS;
std::array<std::uint64_t, BOARD_LENGTH> ZOBRIST_EN_PASSANT_FILE;

struct ZobristInitializer {
  ZobristInitializer() {
    std::random_device device;
    std::seed_seq seed{device(), device(), device(), device()};
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<std::uint64_t> distribution;

    for (auto &color : ZOBRIST_PIECE) {
      for (auto &pieceBitboard : color) {
        for (std::uint64_t &zobrist : pieceBitboard) {
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
