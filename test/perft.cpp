#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <string>

class PerftTest : public ::testing::Test {
protected:
  auto perft(std::uint8_t depth, ChessBoard &board, const std::uint64_t flat)
      -> std::uint64_t {
    if (depth == 0) {
      return 1ULL;
    }

    MoveGenerator whitesGenerator;
    MoveGenerator blacksGenerator;

    whitesGenerator.generatePseudoLegal(board, true);
    blacksGenerator.generatePseudoLegal(board, false);

    UndoCTX undo = {
        .move = {},
        .castlingRights = board.castlingRights,
        .halfmoveClock = board.halfmoveClock,
        .enPassantSquare = board.enPassantSquare,
        .zobrist = board.zobrist,
    };

    const bool forWhites = board.whiteToMove;

    if (forWhites) {
      updateCastlingRights(board, flat, true, blacksGenerator.kills);
      whitesGenerator.appendCastling(board, true);
    } else {
      updateCastlingRights(board, flat, false, whitesGenerator.kills);
      blacksGenerator.appendCastling(board, false);
    }

    std::uint64_t nodes = 0;

    MoveGenerator &generator = forWhites ? whitesGenerator : blacksGenerator;

    for (MoveCTX &move : generator.pseudoLegal) {
      undo.move = move;

      makeMove(board, move);

      const std::uint64_t friendlyFlat = board.getFlat(forWhites);
      const std::uint64_t enemyFlat = board.getFlat(!forWhites);

      if (!board.isKingInCheck(forWhites)) {
        nodes += perft(depth - 1, board, friendlyFlat | enemyFlat);
      }

      undoMove(board, undo);
    }

    return nodes;
  }
};

TEST_F(PerftTest, StartingPosition) {
  static const std::string startingPositionFEN =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard startingBoard(startingPositionFEN);

  EXPECT_EQ(perft(2, startingBoard,
                  startingBoard.getFlat(true) | startingBoard.getFlat(false)),
            400);
}
