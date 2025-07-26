#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <string>

class PerftTest : public ::testing::Test {
protected:
  auto perft(std::uint8_t depth, ChessBoard &board) -> std::uint64_t {
    MoveGenerator whitesGenerator;
    MoveGenerator blacksGenerator;

    if (depth == 0) {
      return 1ULL;
    }

    whitesGenerator.generatePseudoLegal(board, true);
    blacksGenerator.generatePseudoLegal(board, false);

    UndoCTX undo = {
        .move = {},
        .castlingRights = board.castlingRights,
        .halfmoveClock = board.halfmoveClock,
        .enPassantSquare = board.enPassantSquare,
        .zobrist = board.zobrist,
    };

    std::uint64_t flat = board.getFlat(true) | board.getFlat(false);

    // For whites
    updateCastlingRights(board, flat, true, blacksGenerator.kills);
    // For blacks
    updateCastlingRights(board, flat, false, whitesGenerator.kills);

    whitesGenerator.appendCastling(board, true);
    blacksGenerator.appendCastling(board, false);

    std::uint64_t nodes = 0;

    MoveGenerator &enemyGenerator =
        board.whiteToMove ? whitesGenerator : blacksGenerator;

    for (MoveCTX &move : enemyGenerator.pseudoLegal) {
      undo.move = move;

      makeMove(board, move);

      if (!board.isKingUnderCheck(enemyGenerator.kills, !board.whiteToMove)) {
        nodes += perft(depth - 1, board);
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

  EXPECT_EQ(perft(1, startingBoard), 20);
  EXPECT_EQ(perft(2, startingBoard), 400);
  EXPECT_EQ(perft(3, startingBoard), 8902);
  EXPECT_EQ(perft(4, startingBoard), 197281);
  EXPECT_EQ(perft(5, startingBoard), 4865609);
}
