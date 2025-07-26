#include "bitboard.h"
#include "sysifus.h"
#include <array>
#include <cstdint>
#include <random>

extern std::array<
    std::array<std::array<std::uint64_t, BOARD_AREA>, Piece::KING + 1>, 2>
    ZOBRIST_PIECE;
static std::uint64_t ZOBRIST_TURN;
static std::array<std::uint64_t, 1 << 4> ZOBRIST_CASTLING_RIGHTS;
static std::array<std::uint64_t, BOARD_LENGTH> ZOBRIST_EN_PASSANT_FILE;

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
