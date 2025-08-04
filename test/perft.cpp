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
  std::uint64_t enPassants = 0;
  std::uint64_t castles = 0;
};

static auto isEnPassantCapture(const ChessBoard &board, const MoveCTX &move,
                               const bool forWhites) -> bool {
  const std::int32_t capturedPawnSquare =
      forWhites ? board.enPassantSquare - BOARD_LENGTH
                : board.enPassantSquare + BOARD_LENGTH;

  return move.original == Piece::PAWN && board.enPassantSquare != 0 &&
         std::abs(move.from - capturedPawnSquare) == 1 &&
         move.to == board.enPassantSquare;
}

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

    const MoveGenerator &generator =
        forWhites ? whitesGenerator : blacksGenerator;

    for (const auto &move : generator.pseudoLegal) {
      const UndoCTX undo = {
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
        if (isEnPassantCapture(board, move, forWhites)) {
          result.enPassants++;
        }
        if (move.original == KING && std::abs(move.to - move.from) == 2) {
          result.castles++;
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

    const MoveGenerator &generator =
        forWhites ? whitesGenerator : blacksGenerator;

    for (const auto &move : generator.pseudoLegal) {
      const UndoCTX undo = {
          .move = move,
          .castlingRights = board.castlingRights,
          .halfmoveClock = board.halfmoveClock,
          .enPassantSquare = board.enPassantSquare,
          .zobrist = board.zobrist,
      };

      makeMove(board, move);

      if (!board.isKingInCheck(forWhites)) {
        const PerftResult subResult = perft(depth - 1, board);
        totalResult.nodes += subResult.nodes;
        totalResult.captures += subResult.captures;
        totalResult.promotions += subResult.promotions;
        totalResult.checks += subResult.checks;
        totalResult.enPassants += subResult.enPassants;
        totalResult.castles += subResult.castles;
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
  std::cout << "En passants: " << result.enPassants << '\n';
  std::cout << "Castles: " << result.castles << '\n';

  EXPECT_EQ(result.nodes, 197281);
  EXPECT_EQ(result.captures, 1576);
  EXPECT_EQ(result.promotions, 0);
  EXPECT_EQ(result.checks, 469);
  EXPECT_EQ(result.enPassants, 0);
  EXPECT_EQ(result.castles, 0);
}

TEST_F(PerftTest, Kiwipete) {
  static const std::string fen =
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 3 2";
  ChessBoard board(fen);

  const PerftResult result = perft(2, board);

  std::cout << "Nodes: " << result.nodes << '\n';
  std::cout << "Captures: " << result.captures << '\n';
  std::cout << "Promotions: " << result.promotions << '\n';
  std::cout << "Checks: " << result.checks << '\n';
  std::cout << "En passants: " << result.enPassants << '\n';
  std::cout << "Castles: " << result.castles << '\n';

  EXPECT_EQ(result.nodes, 2039);
  EXPECT_EQ(result.captures, 351);
  EXPECT_EQ(result.promotions, 0);
  EXPECT_EQ(result.checks, 3);
  EXPECT_EQ(result.enPassants, 1);
  EXPECT_EQ(result.castles, 91);
}
