#pragma once

#include "bitboard.h"
#include "sysifus.h"
#include <array>
#include <cstdint>

extern std::array<
    std::array<std::array<std::uint64_t, BOARD_AREA>, Piece::KING + 1>, 2>
    ZOBRIST_PIECE;
extern std::array<std::uint64_t, 2> ZOBRIST_TURN;
extern std::array<std::uint64_t, 1 << 4> ZOBRIST_CASTLING_RIGHTS;
extern std::array<std::uint64_t, BOARD_LENGTH> ZOBRIST_EN_PASSANT_FILE;
