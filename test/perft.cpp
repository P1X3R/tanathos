#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include "sysifus.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <string>

struct PerftResult {
  std::uint64_t nodes = 0;
  std::uint64_t captures = 0;
  std::uint64_t promotions = 0;
  std::uint64_t checks = 0;
};

class PerftTest : public ::testing::Test {
protected:
  static auto singlePerft(ChessBoard &board) {
    PerftResult result;
    const bool forWhites = board.whiteToMove;

    MoveGenerator whitesGenerator;
    whitesGenerator.generatePseudoLegal(board, true);
    MoveGenerator blacksGenerator;
    blacksGenerator.generatePseudoLegal(board, false);

    const CastlingRights castlingAttackMask = generateCastlingAttackMask(
        board.getFlat(true) | board.getFlat(false), whitesGenerator.kills,
        blacksGenerator.kills);

    if (forWhites) {
      whitesGenerator.appendCastling(board, castlingAttackMask, true);
    } else {
      blacksGenerator.appendCastling(board, castlingAttackMask, false);
    }

    MoveGenerator &generator = forWhites ? whitesGenerator : blacksGenerator;

    for (const auto &move : generator.pseudoLegal) {
      UndoCTX undo = {
          .move = move,
          .castlingRights = board.castlingRights,
          .halfmoveClock = board.halfmoveClock,
          .enPassantSquare = board.enPassantSquare,
          .zobrist = board.zobrist,
      };

      makeMove(board, move);

      if (!board.isKingInCheck(forWhites)) {
        result.nodes++;
        if (move.captured != Piece::NOTHING) {
          result.captures++;
        }
        if (move.promotion != Piece::NOTHING) {
          result.promotions++;
        }
        if (board.isKingInCheck(!forWhites)) {
          result.checks++;
        }
      }

      undoMove(board, undo);
    }

    return result;
  }

  auto perft(const std::uint8_t depth, ChessBoard &board) -> PerftResult {
    if (depth == 0) {
      return {.nodes = 1, .captures = 0, .promotions = 0, .checks = 0};
    }

    if (depth == 1) {
      return singlePerft(board);
    }

    PerftResult totalResult;

    const bool forWhites = board.whiteToMove;

    MoveGenerator whitesGenerator;
    whitesGenerator.generatePseudoLegal(board, true);
    MoveGenerator blacksGenerator;
    blacksGenerator.generatePseudoLegal(board, false);

    const CastlingRights castlingAttackMask = generateCastlingAttackMask(
        board.getFlat(true) | board.getFlat(false), whitesGenerator.kills,
        blacksGenerator.kills);

    if (forWhites) {
      whitesGenerator.appendCastling(board, castlingAttackMask, true);
    } else {
      blacksGenerator.appendCastling(board, castlingAttackMask, false);
    }

    MoveGenerator &generator = forWhites ? whitesGenerator : blacksGenerator;

    for (const auto &move : generator.pseudoLegal) {
      UndoCTX undo = {
          .move = move,
          .castlingRights = board.castlingRights,
          .halfmoveClock = board.halfmoveClock,
          .enPassantSquare = board.enPassantSquare,
          .zobrist = board.zobrist,
      };

      makeMove(board, move);

      if (!board.isKingInCheck(forWhites)) {
        PerftResult subResult = perft(depth - 1, board);
        totalResult.nodes += subResult.nodes;
        totalResult.captures += subResult.captures;
        totalResult.promotions += subResult.promotions;
        totalResult.checks += subResult.checks;
      }

      undoMove(board, undo);
    }

    return totalResult;
  }
};

TEST_F(PerftTest, StartingPosition) {
  static const std::string startingPositionFEN =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard startingBoard(startingPositionFEN);

  const PerftResult result = perft(4, startingBoard);

  std::cout << "Nodes: " << result.nodes << '\n';
  std::cout << "Captures: " << result.captures << '\n';
  std::cout << "Promotions: " << result.promotions << '\n';
  std::cout << "Checks: " << result.checks << '\n';

  EXPECT_EQ(result.nodes, 197281);
  EXPECT_EQ(result.captures, 1576);
  EXPECT_EQ(result.promotions, 0);
  EXPECT_EQ(result.checks, 469);
}
