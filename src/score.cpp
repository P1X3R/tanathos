#include "bitboard.h"
#include "legalMoves.h"
#include "searching.h"
#include "sysifus.h"
#include <cstdint>

enum MoveOrderBonus : std::uint16_t {
  TRANSPOSITION_TABLE = 20000,
  CAPTURES = 10000,
  KILLER_MOVE = 5000,
  HISTORY = 3000,
};

constexpr std::uint16_t VICTIM_SCALING_FACTOR = 10;
constexpr std::array<int, Piece::NOTHING + 1> PIECE_VALUES = {
    100, 300, 300, 500, 900, 0, 0};

static constexpr std::array<std::array<std::uint16_t, Piece::KING + 1>,
                            Piece::KING + 1>
    MVV_LVA = [] {
      std::array<std::array<std::uint16_t, Piece::KING + 1>, Piece::KING + 1>
          table{};
      constexpr std::uint16_t AGGRESSOR_DECREASING = 6;

      for (int aggressor = 0; aggressor <= Piece::KING; ++aggressor) {
        for (int victim = 0; victim <= Piece::KING; ++victim) {
          table[aggressor][victim] =
              PIECE_VALUES[victim] * VICTIM_SCALING_FACTOR +
              (AGGRESSOR_DECREASING - aggressor);
        }
      }
      return table;
    }();

auto MoveCTX::score(
    const TranspositionTable &TranspositionT,
    const std::array<std::array<MoveCTX, MAX_DEPTH>, 2> &killers,
    const std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>
        &history,
    const std::uint8_t depth, const ChessBoard &board) const -> std::uint16_t {
  std::uint16_t score = 0;

  if (const auto *ttEntry = TranspositionT.probe(board.zobrist)) {
    if (ttEntry->bestMove == *this) {
      score += TRANSPOSITION_TABLE;
    }
  }
  if (captured != Piece::NOTHING) {
    score += CAPTURES + MVV_LVA[original][captured];
  } else {
    score += HISTORY + history[from][to];

    if (killers[depth][0] == *this || killers[depth][1] == *this) {
      score += KILLER_MOVE;
    }
  }
  score += PIECE_VALUES[promotion] * VICTIM_SCALING_FACTOR;

  return score;
}
