#include "perft.h"
#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include <cstdint>
#include <iostream>

auto perft(const std::uint8_t depth, ChessBoard &board, const bool printMoves)
    -> std::uint64_t {
  if (depth == 0) {
    return 1ULL;
  }

  std::uint64_t nodes = 0;

  const bool forWhites = board.whiteToMove;

  MoveGenerator generator;
  generator.generatePseudoLegal(board, forWhites);
  const CastlingRights castlingAttackMask = generateCastlingAttackMask(
      board.getFlat(true) | board.getFlat(false), board);
  generator.appendCastling(board, castlingAttackMask, forWhites);

  for (const auto &move : generator.pseudoLegal) {
    const UndoCTX undo(move, board);

    makeMove(board, move);

    if (!board.isKingInCheck(forWhites)) {
      const std::uint64_t leafNodes = perft(depth - 1, board, false);
      if (printMoves) {
        std::cout << moveToUCI(move) << ": " << leafNodes << '\n';
      }
      nodes += leafNodes;
    }

    undoMove(board, undo);
  }

  return nodes;
}
