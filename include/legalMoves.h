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
  Piece original = NOTHING, captured = NOTHING, promotion = NOTHING;
};

class MoveGenerator {
public:
  std::vector<MoveCTX> pseudoLegal, legal;

  void generatePseudoLegal(const ChessBoard &board, bool forWhites);
  void filterLegal(const ChessBoard &board, bool forWhites);

private:
  [[nodiscard]] auto isKingInCheck() const -> bool;
};
