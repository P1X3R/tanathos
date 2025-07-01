#pragma once

#include "sysifus.h"
#include <array>
#include <cstdint>
#include <functional>
#include <numeric>

struct ChessBoard {
  std::array<uint64_t, Piece::KING + 1> whites, blacks;
  uint64_t zobrist;
  uint32_t halfmoveCounter : 7, enPassantSquare : 6; // 0 means "no en passant"
  uint32_t whiteToMove : 1;
  union {
    struct {
      uint32_t whiteKingSide : 1, whiteQueenSide : 1, blackKingSide : 1,
          blackQueenSide : 1;
    };
    uint32_t compressed : 4;
  } castlingRights;

  [[nodiscard]] auto getFlat(const bool forWhites) const -> uint64_t {
    const std::array<uint64_t, Piece::KING + 1> &color =
        forWhites ? this->whites : this->blacks;

    return std::reduce(color.begin(), color.end(), 0, std::bit_or{});
  }
};
