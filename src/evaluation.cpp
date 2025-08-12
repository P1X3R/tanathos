#include "board.h"
#include "searching.h"
#include <cstdint>

auto ChessBoard::evaluate() const -> std::int32_t {
  std::int32_t score = 0;

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    score += std::popcount(whites[type]) * PIECE_VALUES[type];
    score -= std::popcount(blacks[type]) * PIECE_VALUES[type];
  }

  return score;
}
