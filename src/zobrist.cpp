#include "bitboard.h"
#include "sysifus.h"
#include <chrono>
#include <cstdint>
#include <random>

static std::array<std::uint64_t, BOARD_AREA> ZOBRIST_SQUARE;
static std::array<std::uint64_t, Piece::KING + 1> ZOBRIST_PIECE;
static std::array<std::uint64_t, 2> ZOBRIST_TURN;
static std::array<std::uint64_t, 1 << 4> ZOBRIST_CASTLING_RIGHTS;
static std::array<std::uint64_t, BOARD_LENGTH> ZOBRIST_EN_PASSANT_FILE;

struct ZobristInitializer {
  ZobristInitializer() {
    std::random_device device;
    std::minstd_rand gen(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<std::uint64_t> distribution;

    for (std::uint64_t &zobrist : ZOBRIST_SQUARE) {
      zobrist = distribution(gen);
    }

    for (std::uint64_t &zobrist : ZOBRIST_PIECE) {
      zobrist = distribution(gen);
    }

    for (std::uint64_t &zobrist : ZOBRIST_TURN) {
      zobrist = distribution(gen);
    }

    for (std::uint64_t &zobrist : ZOBRIST_CASTLING_RIGHTS) {
      zobrist = distribution(gen);
    }

    for (std::uint64_t &zobrist : ZOBRIST_EN_PASSANT_FILE) {
      zobrist = distribution(gen);
    }
  }
};

static ZobristInitializer zobristInitializerInstance;
