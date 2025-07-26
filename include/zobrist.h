#pragma once

#include "bitboard.h"
#include "sysifus.h"
#include <array>
#include <cstdint>

static std::array<
    std::array<std::array<std::uint64_t, BOARD_AREA>, Piece::KING + 1>, 2>
    ZOBRIST_PIECE;
static std::uint64_t ZOBRIST_TURN;
static std::array<std::uint64_t, 1 << 4> ZOBRIST_CASTLING_RIGHTS;
static std::array<std::uint64_t, BOARD_LENGTH> ZOBRIST_EN_PASSANT_FILE;
