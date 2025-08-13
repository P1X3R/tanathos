#include "bitboard.h"
#include "board.h"
#include "psqt.h"
#include "searching.h"
#include <cstdint>

auto ChessBoard::evaluate() const -> std::int32_t {
  std::int32_t score = 0;

  // Iterate through all piece types
  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {

    for (std::uint32_t square = 0; square < BOARD_AREA; square++) {

      if ((whites[type] & (1ULL << square)) != 0) {
        score += PIECE_VALUES[type];
        score += PSQT[type][square];
      }

      const std::uint32_t flippedSquare = square ^ (BOARD_AREA - BOARD_LENGTH);

      if ((blacks[type] & (1ULL << square)) != 0) {
        score -= PIECE_VALUES[type];
        score -= PSQT[type][flippedSquare];
      }
    }

    // Evaluate black pieces
    for (std::uint32_t square = 0; square < BOARD_AREA; square++) {
    }
  }

  return score;
}
