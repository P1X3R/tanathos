#pragma once

#include "board.h"
#include "sysifus.h"
#include <cstdint>
#include <vector>

struct MoveCTX {
  uint32_t from : 6 = 0; // The square where the moved piece comes from
  uint32_t to : 6 = 0;   // The square where the moved piece shall land
  // The square where the captured piece was. Useful for en passant handling
  uint32_t capturedSquare : 6 = 0;
  Piece original = Piece::NOTHING, captured = Piece::NOTHING,
        promotion = Piece::NOTHING;
};

void appendContext(MoveCTX &ctx, bool forWhites,
                   const std::array<uint64_t, Piece::KING + 1> &enemyColor,
                   uint64_t enemyFlat, std::vector<MoveCTX> &pseudoLegal,
                   int32_t enPassantSquare);

// For only one color
class MoveGenerator {
public:
  std::vector<MoveCTX> pseudoLegal, legal;
  uint64_t kills; // Useful for castling updating

  void generatePseudoLegal(const ChessBoard &board, bool forWhites);
  void filterLegal(const ChessBoard &board, bool forWhites);

private:
  // Updates castling rights based on path obstruction only.
  // Assumes other castling conditions (king/rook haven't moved,
  // king not in check) are handled elsewhere.
  static void updateCastlingRights(ChessBoard &board, uint64_t enemyFlat,
                                   bool forWhites, uint64_t enemyAttacks);

  void appendCastling(const ChessBoard &board, bool forWhites);
};
