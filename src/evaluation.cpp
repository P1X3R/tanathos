#include "bitboard.h"
#include "board.h"
#include "psqt.h"
#include "searching.h"
#include <bit>
#include <cstdint>

auto ChessBoard::evaluate() const -> std::int32_t {
  std::int32_t result = 0;

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    std::uint64_t bitboard = whites[type];
    while (bitboard != 0) {
      // Flip for whites because in PSQT A8=0, but engine uses A1=0
      const std::uint32_t square =
          std::countr_zero(bitboard) ^ (BOARD_AREA - BOARD_LENGTH);
      result += PIECE_VALUES[type] + PSQT[type][square];
      bitboard &= bitboard - 1;
    }

    bitboard = blacks[type];
    while (bitboard != 0) {
      const std::uint32_t square = std::countr_zero(bitboard);
      result -= PIECE_VALUES[type] + PSQT[type][square];
      bitboard &= bitboard - 1;
    }
  }

  return result;
}
