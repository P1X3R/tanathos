#pragma once

#include "board.h"
#include <cstdint>

auto perft(std::uint8_t depth, ChessBoard &board, bool printMoves)
    -> std::uint64_t;