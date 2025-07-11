#pragma once

#include "board.h"
#include "sysifus.h"
#include <cstdint>
#include <vector>

struct MoveCTX {
  std::uint32_t from : 6 = 0; // The square where the moved piece comes from
  std::uint32_t to : 6 = 0;   // The square where the moved piece shall land
  // The square where the captured piece was. Useful for en passant handling
  std::uint32_t capturedSquare : 6 = 0;
  Piece original = Piece::NOTHING, captured = Piece::NOTHING,
        promotion = Piece::NOTHING;
};

auto fromAlgebraic() -> MoveCTX;

void appendContext(MoveCTX &ctx, bool forWhites,
                   const std::array<std::uint64_t, Piece::KING + 1> &enemyColor,
                   std::uint64_t enemyFlat, std::vector<MoveCTX> &pseudoLegal,
                   std::int32_t enPassantSquare);

// Updates castling rights based on path obstruction only.
// Assumes other castling conditions (king/rook haven't moved,
// king not in check) are handled elsewhere.
void updateCastlingRights(ChessBoard &board, std::uint64_t enemyFlat,
                          bool forWhites, std::uint64_t enemyAttacks);

constexpr std::uint32_t MAX_MOVES_IN_A_POSITION = 256;

// For only one color
struct MoveGenerator {
  std::vector<MoveCTX> pseudoLegal;
  std::uint64_t kills; // Useful for castling updating

  MoveGenerator() { pseudoLegal.reserve(MAX_MOVES_IN_A_POSITION); }

  void generatePseudoLegal(const ChessBoard &board, bool forWhites);

  void appendCastling(const ChessBoard &board, bool forWhites);
};
