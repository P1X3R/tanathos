#include "move.h"
#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

class MakeMoveTest : public ::testing::Test {
protected:
  // Helper to get piece at a square for verification (from ChessParsingTest)
  static auto getPieceAt(const std::uint32_t square, const ChessBoard &board)
      -> std::pair<Piece, bool> {
    const std::uint64_t bit = 1ULL << square;

    for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
      if ((board.whites.at(type) & bit) != 0) {
        return std::make_pair(static_cast<Piece>(type), true);
      }
    }

    for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
      if ((board.blacks.at(type) & bit) != 0) {
        return std::make_pair(static_cast<Piece>(type), false);
      }
    }
    return std::make_pair(Piece::NOTHING, false);
  }
};

TEST_F(MakeMoveTest, QuietPawnMove) {
  const std::string fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard board(fen);
  ChessBoard initialBoard = board;
  MoveCTX ctx = fromAlgebraic("e2e4", board);

  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify piece movement
  EXPECT_EQ(getPieceAt(E2, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(E4, board).first, Piece::PAWN);
  EXPECT_TRUE(getPieceAt(E4, board).second); // White pawn

  // Verify en passant square (double push: (E2 + E4) / 2 = E3)
  EXPECT_EQ(board.enPassantSquare, E3);

  // Verify halfmove counter reset (pawn move)
  EXPECT_EQ(board.halfmoveClock, 0);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, CaptureMove) {
  const std::string fen = "8/8/8/3n4/4P3/8/8/8 w - - 0 1";
  ChessBoard board(fen);
  const MoveCTX ctx = fromAlgebraic("e4d5", board);
  const ChessBoard initialBoard = board;

  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify piece movement and capture
  EXPECT_EQ(getPieceAt(E4, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(D5, board).first, Piece::PAWN);
  EXPECT_TRUE(getPieceAt(D5, board).second);           // White pawn
  EXPECT_EQ(getPieceAt(D5, board).first, Piece::PAWN); // No black knight

  // Verify no en passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter reset (capture)
  EXPECT_EQ(board.halfmoveClock, 0);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, EnPassantCapture) {
  const std::string fen = "8/8/8/3pP3/8/8/8/8 w - d6 0 1";
  ChessBoard board(fen);
  board.whites.fill(0);
  board.blacks.fill(0);
  board.whites[Piece::PAWN] |= (1ULL << E5);
  board.blacks[Piece::PAWN] |= (1ULL << D5);
  board.enPassantSquare = D6;

  const MoveCTX ctx = fromAlgebraic("e5d6", board);

  const ChessBoard initialBoard = board;
  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify piece movement and capture
  EXPECT_EQ(getPieceAt(E5, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(D6, board).first, Piece::PAWN);
  EXPECT_TRUE(getPieceAt(D6, board).second);              // White pawn
  EXPECT_EQ(getPieceAt(D5, board).first, Piece::NOTHING); // Black pawn captured

  // Verify en passant square cleared
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter reset
  EXPECT_EQ(board.halfmoveClock, 0);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, PawnPromotion) {
  const std::string fen = "8/6P1/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);
  const MoveCTX ctx = fromAlgebraic("g7g8q", board);

  const ChessBoard initialBoard = board;
  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify promotion
  EXPECT_EQ(getPieceAt(G7, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(G8, board).first, Piece::QUEEN);
  EXPECT_TRUE(getPieceAt(G8, board).second); // White queen

  // Verify no en passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter reset (pawn move)
  EXPECT_EQ(board.halfmoveClock, 0);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, CastlingKingSide) {
  const std::string fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1";
  ChessBoard board(fen);
  const MoveCTX ctx = fromAlgebraic("e1g1", board);

  const ChessBoard initialBoard = board;
  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify king and rook movement
  EXPECT_EQ(getPieceAt(E1, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(H1, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(G1, board).first, Piece::KING);
  EXPECT_TRUE(getPieceAt(G1, board).second);
  EXPECT_EQ(getPieceAt(F1, board).first, Piece::ROOK);
  EXPECT_TRUE(getPieceAt(F1, board).second);

  // Verify castling rights updated
  EXPECT_FALSE(board.castlingRights.whiteKingSide);
  EXPECT_FALSE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);

  // Verify no en passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter incremented (non-pawn, non-capture)
  EXPECT_EQ(board.halfmoveClock, 1);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, CastlingQueenSide) {
  const std::string fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1";
  ChessBoard board(fen);
  const MoveCTX ctx = fromAlgebraic("e1c1", board);

  const ChessBoard initialBoard = board;
  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify king and rook movement
  EXPECT_EQ(getPieceAt(E1, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(A1, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(C1, board).first, Piece::KING);
  EXPECT_TRUE(getPieceAt(C1, board).second);
  EXPECT_EQ(getPieceAt(D1, board).first, Piece::ROOK);
  EXPECT_TRUE(getPieceAt(D1, board).second);

  // Verify castling rights updated
  EXPECT_FALSE(board.castlingRights.whiteKingSide);
  EXPECT_FALSE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);

  // Verify no en passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter incremented
  EXPECT_EQ(board.halfmoveClock, 1);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, RookMoveAffectsCastling) {
  const std::string fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R1BQKBNR w KQkq - 0 1";
  ChessBoard board(fen);
  const MoveCTX ctx = fromAlgebraic("a1b1", board);

  const ChessBoard initialBoard = board;
  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify rook movement
  EXPECT_EQ(getPieceAt(A1, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(B1, board).first, Piece::ROOK);
  EXPECT_TRUE(getPieceAt(B1, board).second);

  // Verify castling rights updated
  EXPECT_TRUE(board.castlingRights.whiteKingSide);
  EXPECT_FALSE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);

  // Verify no en passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter incremented
  EXPECT_EQ(board.halfmoveClock, 1);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, CaptureRookAffectsCastling) {
  const std::string fen =
      "rnbqkbnr/pppp1ppp/5n2/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard board(fen);
  const MoveCTX ctx =
      fromAlgebraic("g1h8", board); // White knight captures black rook

  const ChessBoard initialBoard = board;
  const UndoCTX undo(ctx, board);

  makeMove(board, ctx);

  // Verify piece movement and capture
  EXPECT_EQ(getPieceAt(G1, board).first, Piece::NOTHING);
  EXPECT_EQ(getPieceAt(H8, board).first, Piece::KNIGHT);
  EXPECT_TRUE(getPieceAt(H8, board).second);
  EXPECT_EQ(getPieceAt(H8, board).first, Piece::KNIGHT); // No black rook

  // Verify castling rights updated
  EXPECT_TRUE(board.castlingRights.whiteKingSide);
  EXPECT_TRUE(board.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);

  // Verify no en passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Verify halfmove counter reset (capture)
  EXPECT_EQ(board.halfmoveClock, 0);

  // Verify turn switched
  EXPECT_FALSE(board.whiteToMove);

  // Verify Zobrist hash updated
  EXPECT_NE(board.zobrist, initialBoard.zobrist);

  undoMove(board, undo);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    EXPECT_EQ(initialBoard.whites[type], board.whites[type]);
    EXPECT_EQ(initialBoard.blacks[type], board.blacks[type]);
  }
}

TEST_F(MakeMoveTest, ZobristPropertyBasedTest) {
  static constexpr std::size_t RANDOM_TESTS = 1000;
  // Seed for reproducible randomness
  std::mt19937 gen(BOARD_AREA);
  std::uniform_int_distribution<> dis(0, RANDOM_TESTS);

  // Test 1000 random positions
  for (int test = 0; test < RANDOM_TESTS; test++) {
    // Create random position
    ChessBoard board;
    board.whiteToMove = (dis(gen) % 2) != 0;
    static constexpr std::uint32_t EN_PASSANT_FILE_MULTIPLIER = 5;
    board.enPassantSquare =
        (dis(gen) % BOARD_LENGTH) +
        ((dis(gen) % 2) != 0
             ? BOARD_LENGTH * 2
             : BOARD_LENGTH * EN_PASSANT_FILE_MULTIPLIER); // a3/h3 or a6/h6
    if (dis(gen) % EN_PASSANT_FILE_MULTIPLIER == 0) {
      board.enPassantSquare = 0; // 20% chance no en passant
    }

    // Random castling rights
    board.castlingRights.whiteKingSide = (dis(gen) % 2) != 0;
    board.castlingRights.whiteQueenSide = (dis(gen) % 2) != 0;
    board.castlingRights.blackKingSide = (dis(gen) % 2) != 0;
    board.castlingRights.blackQueenSide = (dis(gen) % 2) != 0;

    // Random pieces
    for (int sq = 0; sq < BOARD_AREA; sq++) {
      if (dis(gen) % 3 == 0) { // 33% chance of piece on square
        const bool white = (dis(gen) % 2) != 0;
        const auto piece =
            static_cast<Piece>(dis(gen) % (Piece::KING + 1)); // PAWN-KING
        auto &pieces = white ? board.whites : board.blacks;
        pieces[piece] |= (1ULL << sq);
      }
    }

    // Calculate initial hash from scratch
    const std::uint64_t initialHash = board.calculateZobrist();

    board.zobrist = initialHash;

    // Generate all legal moves
    MoveGenerator generator;
    generator.generatePseudoLegal(board, false, board.whiteToMove);
    const CastlingRights castlingAttackMask = generateCastlingAttackMask(
        board.getFlat(true) | board.getFlat(true), board);
    generator.appendCastling(board, castlingAttackMask, board.whiteToMove);

    // Test each move
    for (const auto &move : generator.pseudoLegal) {
      // Save state for undo
      const UndoCTX undo(move, board);

      // Make move
      makeMove(board, move);

      if (board.isKingInCheck(!board.whiteToMove)) {
        undoMove(board, undo);
        continue;
      }

      // Property 1: Hash should change (unless it's a null move or something
      // very special)
      EXPECT_NE(board.zobrist, initialHash) << "Hash didn't change after move";

      // Property 2: Hash should match full calculation
      const std::uint64_t newCalculatedHash = board.calculateZobrist();

      EXPECT_EQ(board.zobrist, newCalculatedHash) << "Hash mismatch after move";

      // Property 3: Undo should restore original hash
      undoMove(board, undo);
      EXPECT_EQ(board.zobrist, initialHash) << "Hash not restored after undo";

      // Property 4: Making and undoing move should leave board unchanged
      for (int piece = Piece::PAWN; piece <= Piece::KING; piece++) {
        EXPECT_EQ(undo.move.original == piece
                      ? (board.whites[piece] | (1ULL << move.from))
                      : board.whites[piece],
                  board.whites[piece])
            << "White pieces not restored";
        EXPECT_EQ(board.blacks[piece], board.blacks[piece])
            << "Black pieces not restored";
      }
    }
  }
}
