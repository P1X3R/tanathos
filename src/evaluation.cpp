#include "bitboard.h"
#include "board.h"
#include "psqt.h"
#include "searching.h"
#include <bit>
#include <cstdint>

auto ChessBoard::evaluate() const -> std::int32_t {
  static constexpr std::array<std::uint8_t, Piece::QUEEN + 1> PHASE_VALUES = {
      0, 1, 1, 2, 4};
  static constexpr std::uint8_t TOTAL_PHASE =
      (PHASE_VALUES[Piece::KNIGHT] * 2) + (PHASE_VALUES[Piece::BISHOP] * 2) +
      (PHASE_VALUES[Piece::ROOK] * 2) + PHASE_VALUES[Piece::QUEEN];

  std::int32_t phase = TOTAL_PHASE;

  static auto incrementCount =
      [&](const std::array<std::uint64_t, Piece::KING + 1> &color) {
        for (std::uint32_t type = Piece::PAWN; type <= Piece::QUEEN; type++) {
          phase -= std::popcount(color[type]) * PHASE_VALUES[type];
        }
      };

  incrementCount(whites);
  incrementCount(blacks);

  static constexpr std::int32_t PHASE_SCALING_FACTOR = 256;
  phase = (phase * PHASE_SCALING_FACTOR + (TOTAL_PHASE / 2)) / TOTAL_PHASE;

  std::int32_t midgame = 0;
  std::int32_t endgame = 0;

  for (std::uint32_t type = Piece::PAWN; type <= Piece::QUEEN; type++) {
    std::uint64_t bitboard = whites[type];

    while (bitboard != 0) {
      std::int32_t square =
          std::countr_zero(bitboard) ^ (BOARD_AREA - BOARD_LENGTH);
      midgame += MIDGAME_PSQT[type][square] + PIECE_VALUES[type];
      endgame += ENDGAME_PSQT[type][square] + PIECE_VALUES[type];
      bitboard &= bitboard - 1;
    }

    bitboard = blacks[type];
    while (bitboard != 0) {
      std::int32_t square = std::countr_zero(bitboard);
      midgame -= MIDGAME_PSQT[type][square] + PIECE_VALUES[type];
      endgame -= ENDGAME_PSQT[type][square] + PIECE_VALUES[type];
      bitboard &= bitboard - 1;
    }
  }

  return ((midgame * (PHASE_SCALING_FACTOR - phase)) + (endgame * phase)) /
         PHASE_SCALING_FACTOR;
}
