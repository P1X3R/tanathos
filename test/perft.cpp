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

    updateCastlingRights(board, board.getFlat(false), true,
                         blacksGenerator.kills); // For whites
    updateCastlingRights(board, board.getFlat(true), false,
                         whitesGenerator.kills); // For blacks

    whitesGenerator.appendCastling(board, true);
    blacksGenerator.appendCastling(board, false);

    std::uint64_t nodes = 0;

    MoveGenerator &enemyGenerator =
        board.whiteToMove ? whitesGenerator : blacksGenerator;

    for (MoveCTX &move : enemyGenerator.pseudoLegal) {
      undo.move = move;

      makeMove(board, move);

      if (!board.isKingUnderCheck(enemyGenerator.kills, !board.whiteToMove)) {
        std::cout << "DEBUG Move - "
                  << "From: " << move.from << ", To: " << move.to
                  << ", CapturedSquare: " << move.capturedSquare
                  << ", OriginalPiece: " << static_cast<int>(move.original)
                  << ", CapturedPiece: " << static_cast<int>(move.captured)
                  << ", PromotionPiece: " << static_cast<int>(move.promotion)
                  << '\n';
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
}
