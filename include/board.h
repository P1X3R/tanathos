#pragma once

#include "sysifus.h"
#include <array>
#include <bit>
#include <bitset>
#include <cstdint>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>

struct ChessBoard {
  std::array<std::uint64_t, Piece::KING + 1> whites, blacks;
  std::uint64_t zobrist;
  std::uint32_t halfmoveCounter : 7;
  std::uint32_t enPassantSquare : 6; // 0 means "no en passant"
  bool whiteToMove : 1;
  struct CastlingRight {
    bool whiteKingSide : 1;
    bool whiteQueenSide : 1;
    bool blackKingSide : 1;
    bool blackQueenSide : 1;
  } castlingRights;

  ChessBoard() = default;
  explicit ChessBoard(const std::string &fen);

  [[nodiscard]] auto getFlat(const bool forWhites) const -> std::uint64_t {
    const std::array<std::uint64_t, Piece::KING + 1> &color =
        forWhites ? this->whites : this->blacks;

    return std::reduce(color.begin(), color.end(), 0, std::bit_or{});
  }

  [[nodiscard]] auto getCompressedCastlingRights() const -> std::uint8_t {
    // Masked to ensure only the first 4 bits can be set
    static constexpr std::uint8_t irrelevantBitMask = 0x0F;
    return std::bit_cast<std::uint8_t>(castlingRights) & irrelevantBitMask;
  }
};

enum BoardSquare : std::uint8_t {
  A1 = 0,
  B1,
  C1,
  D1,
  E1,
  F1,
  G1,
  H1,
  A2,
  B2,
  C2,
  D2,
  E2,
  F2,
  G2,
  H2,
  A3,
  B3,
  C3,
  D3,
  E3,
  F3,
  G3,
  H3,
  A4,
  B4,
  C4,
  D4,
  E4,
  F4,
  G4,
  H4,
  A5,
  B5,
  C5,
  D5,
  E5,
  F5,
  G5,
  H5,
  A6,
  B6,
  C6,
  D6,
  E6,
  F6,
  G6,
  H6,
  A7,
  B7,
  C7,
  D7,
  E7,
  F7,
  G7,
  H7,
  A8,
  B8,
  C8,
  D8,
  E8,
  F8,
  G8,
  H8
};
