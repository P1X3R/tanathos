#include "searching.h"
#include "legalMoves.h"
#include "move.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>

static constexpr std::int32_t INF = INT32_MAX;
static constexpr std::int32_t CHECKMATE_SCORE = 50000;

auto Searching::search(std::uint8_t depth) -> std::int32_t {
  if (depth == 0) {
    return board.evaluate();
  }
  if (board.isDraw(zobristHistory)) {
    return 0;
  }

  std::int32_t max = -INF;

  const bool forWhites = board.whiteToMove;

  MoveGenerator generator;
  generator.generatePseudoLegal(board, forWhites);
  const CastlingRights castlingAttackMask = generateCastlingAttackMask(
      board.getFlat(true) | board.getFlat(false), board);
  generator.appendCastling(board, castlingAttackMask, forWhites);

  bool hasLegalMoves = false;
  for (std::size_t moveIndex = 0; moveIndex < generator.pseudoLegal.size();
       ++moveIndex) {
    const MoveCTX *move =
        pickMove(generator.pseudoLegal, moveIndex, board, depth);
    if (move == nullptr) {
      break;
    }

    const UndoCTX undo = {
        .move = *move,
        .castlingRights = board.castlingRights,
        .halfmoveClock = board.halfmoveClock,
        .enPassantSquare = board.enPassantSquare,
        .zobrist = board.zobrist,
    };

    makeMove(board, *move);

    if (board.isKingInCheck(forWhites)) {
      undoMove(board, undo);
      continue;
    }

    hasLegalMoves = true;

    std::int32_t score = -search(depth - 1);
    max = std::max(score, max);

    undoMove(board, undo);
  }

  if (!hasLegalMoves) {
    const std::int32_t outcomeScoreMagnitude =
        board.isKingInCheck(forWhites) ? CHECKMATE_SCORE - depth : 0;
    return forWhites ? outcomeScoreMagnitude : -outcomeScoreMagnitude;
  }

  return max;
}
