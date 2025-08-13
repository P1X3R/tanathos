#include "bitboard.h"
#include "legalMoves.h"
#include "searching.h"
#include "sysifus.h"
#include <cstddef>
#include <cstdint>

enum MoveOrderBonus : std::uint16_t {
  TRANSPOSITION_TABLE = 50000,
  CAPTURES = 10000,
  KILLER_MOVE = 5000,
  HISTORY = 3000,
};

constexpr std::uint16_t VICTIM_SCALING_FACTOR = 10;

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
    const MoveCTX *entryBestMove,
    const std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> &killers,
    const std::array<
        std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>, 2>
        &history,
    const std::uint8_t ply, const ChessBoard &board) const -> std::uint16_t {
  std::uint16_t score = 0;

  if (entryBestMove != nullptr && *entryBestMove == *this) {
    score += TRANSPOSITION_TABLE;
  }
  if (captured != Piece::NOTHING) {
    score += CAPTURES + MVV_LVA[original][captured];
  } else {
    score += HISTORY +
             history[static_cast<std::size_t>(!board.whiteToMove)][from][to];

    if (killers[ply][0] == *this || killers[ply][1] == *this) {
      score += KILLER_MOVE;
    }
  }
  score += PIECE_VALUES[promotion] * VICTIM_SCALING_FACTOR;

  return score;
}

[[nodiscard]] auto
Searching::pickMove(std::vector<MoveCTX> &moves, std::uint8_t moveIndex,
                    const ChessBoard &board, std::uint8_t ply,
                    const MoveCTX *entryBestMove) -> const MoveCTX * {
  if (moves.empty()) {
    return nullptr;
  }

  std::int32_t bestMoveScore = -1;
  std::int16_t bestMoveIndex = -1;

  for (std::size_t i = moveIndex; i < moves.size(); i++) {
    const MoveCTX &move = moves[i];
    const std::uint16_t moveScore =
        move.score(entryBestMove, killers, history, ply, board);

    if (moveScore > bestMoveScore) {
      bestMoveScore = moveScore;
      bestMoveIndex = static_cast<std::int16_t>(i);
    }
  }

  if (bestMoveIndex != -1) {
    std::swap(moves[bestMoveIndex], moves[moveIndex]);
  }

  return &moves[moveIndex];
}
