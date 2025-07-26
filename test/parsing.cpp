#include "board.h"
// #include "bitboard.h"
#include "legalMoves.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <string>

// For debugging
void printBitboard(const std::uint64_t bitboard, const char icon) {
  std::cout << "  A B C D E F G H\n";
  for (int8_t rank = BOARD_LENGTH - 1; rank >= 0; rank--) {
    std::cout << rank + 1 << ' ';
    for (int8_t file = 0; file < BOARD_LENGTH; file++) {
      const bool isBitSet =
          (bitboard & (1ULL << ((rank * BOARD_LENGTH) + file))) != 0;
      std::cout << (isBitSet ? icon : '.') << ' ';
    }
    std::cout << '\n';
  }
}

// --- GTest Test Fixture for ChessBoard and MoveCTX parsing ---
class ChessParsingTest : public ::testing::Test {
protected:
  // Helper to get piece at a square for verification
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

TEST_F(ChessParsingTest, ChessBoardInitialFEN) {
  const std::string fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard board(fen);

  // Test piece placement for White
  EXPECT_EQ(getPieceAt(A1, board).first, Piece::ROOK);
  EXPECT_TRUE(getPieceAt(A1, board).second);
  EXPECT_EQ(getPieceAt(B1, board).first, Piece::KNIGHT);
  EXPECT_TRUE(getPieceAt(B1, board).second);
  EXPECT_EQ(getPieceAt(C1, board).first, Piece::BISHOP);
  EXPECT_TRUE(getPieceAt(C1, board).second);
  EXPECT_EQ(getPieceAt(D1, board).first, Piece::QUEEN);
  EXPECT_TRUE(getPieceAt(D1, board).second);
  EXPECT_EQ(getPieceAt(E1, board).first, Piece::KING);
  EXPECT_TRUE(getPieceAt(E1, board).second);
  EXPECT_EQ(getPieceAt(F1, board).first, Piece::BISHOP);
  EXPECT_TRUE(getPieceAt(F1, board).second);
  EXPECT_EQ(getPieceAt(G1, board).first, Piece::KNIGHT);
  EXPECT_TRUE(getPieceAt(G1, board).second);
  EXPECT_EQ(getPieceAt(H1, board).first, Piece::ROOK);
  EXPECT_TRUE(getPieceAt(H1, board).second);

  for (int i = A2; i <= H2; ++i) {
    EXPECT_EQ(getPieceAt(i, board).first, Piece::PAWN);
    EXPECT_TRUE(getPieceAt(i, board).second);
  }

  // Test piece placement for Black
  EXPECT_EQ(getPieceAt(A8, board).first, Piece::ROOK);
  EXPECT_FALSE(getPieceAt(A8, board).second);
  EXPECT_EQ(getPieceAt(B8, board).first, Piece::KNIGHT);
  EXPECT_FALSE(getPieceAt(B8, board).second);
  EXPECT_EQ(getPieceAt(C8, board).first, Piece::BISHOP);
  EXPECT_FALSE(getPieceAt(C8, board).second);
  EXPECT_EQ(getPieceAt(D8, board).first, Piece::QUEEN);
  EXPECT_FALSE(getPieceAt(D8, board).second);
  EXPECT_EQ(getPieceAt(E8, board).first, Piece::KING);
  EXPECT_FALSE(getPieceAt(E8, board).second);
  EXPECT_EQ(getPieceAt(F8, board).first, Piece::BISHOP);
  EXPECT_FALSE(getPieceAt(F8, board).second);
  EXPECT_EQ(getPieceAt(G8, board).first, Piece::KNIGHT);
  EXPECT_FALSE(getPieceAt(G8, board).second);
  EXPECT_EQ(getPieceAt(H8, board).first, Piece::ROOK);
  EXPECT_FALSE(getPieceAt(H8, board).second);

  for (int i = A7; i <= H7; ++i) {
    EXPECT_EQ(getPieceAt(i, board).first, Piece::PAWN);
    EXPECT_FALSE(getPieceAt(i, board).second);
  }

  // Test active color
  EXPECT_TRUE(board.whiteToMove);

  // Test castling rights
  EXPECT_TRUE(board.castlingRights.whiteKingSide);
  EXPECT_TRUE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);

  // Test en passant square
  EXPECT_EQ(board.enPassantSquare, 0); // No en passant

  // Test halfmove clock
  EXPECT_EQ(board.halfmoveClock, 0);
}

TEST_F(ChessParsingTest, ChessBoardEmptyBoard) {
  const std::string fen = "8/8/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);

  // All bitboards should be empty
  for (int i = 0; i <= Piece::KING; ++i) {
    EXPECT_EQ(board.whites.at(i), 0ULL);
    EXPECT_EQ(board.blacks.at(i), 0ULL);
  }

  // Active color
  EXPECT_TRUE(board.whiteToMove);

  // Castling rights
  EXPECT_FALSE(board.castlingRights.whiteKingSide);
  EXPECT_FALSE(board.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board.castlingRights.blackKingSide);
  EXPECT_FALSE(board.castlingRights.blackQueenSide);

  // En passant square
  EXPECT_EQ(board.enPassantSquare, 0);

  // Halfmove clock
  EXPECT_EQ(board.halfmoveClock, 0);
}

TEST_F(ChessParsingTest, ChessBoardCustomFEN) {
  // Example FEN from a game: 2rqkbn1/pp2pppp/3p4/8/3QP3/8/PP3PPP/RNB1KBNR b KQq
  // d3 0 3
  const std::string fen =
      "2rqkbn1/pp2pppp/3p4/8/3QP3/8/PP3PPP/RNB1KBNR b KQq d3 0 3";
  ChessBoard board(fen);

  // Test piece placement (spot check a few)
  EXPECT_EQ(getPieceAt(D8, board).first, Piece::QUEEN);
  EXPECT_FALSE(getPieceAt(D8, board).second); // Black Queen
  EXPECT_EQ(getPieceAt(D4, board).first, Piece::QUEEN);
  EXPECT_TRUE(getPieceAt(D4, board).second); // White Queen
  EXPECT_EQ(getPieceAt(D6, board).first, Piece::PAWN);
  EXPECT_FALSE(getPieceAt(D6, board).second); // Black Pawn

  // Active color
  EXPECT_FALSE(board.whiteToMove); // Black to move

  // Castling rights
  EXPECT_TRUE(board.castlingRights.whiteKingSide);
  EXPECT_TRUE(board.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);

  // En passant square: d3 -> (rank 2, file 3) -> 2*8 + 3 = 19
  EXPECT_EQ(board.enPassantSquare, D3);

  // Halfmove clock
  EXPECT_EQ(board.halfmoveClock, 0);
}

TEST_F(ChessParsingTest, ChessBoardCastlingRightsVariations) {
  // No castling
  ChessBoard board1("8/8/8/8/8/8/8/8 w - - 0 1");
  EXPECT_FALSE(board1.castlingRights.whiteKingSide);
  EXPECT_FALSE(board1.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board1.castlingRights.blackKingSide);
  EXPECT_FALSE(board1.castlingRights.blackQueenSide);

  // All castling
  ChessBoard board2("8/8/8/8/8/8/8/8 w KQkq - 0 1");
  EXPECT_TRUE(board2.castlingRights.whiteKingSide);
  EXPECT_TRUE(board2.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board2.castlingRights.blackKingSide);
  EXPECT_TRUE(board2.castlingRights.blackQueenSide);

  // Only white king side
  ChessBoard board3("8/8/8/8/8/8/8/8 w K - 0 1");
  EXPECT_TRUE(board3.castlingRights.whiteKingSide);
  EXPECT_FALSE(board3.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board3.castlingRights.blackKingSide);
  EXPECT_FALSE(board3.castlingRights.blackQueenSide);

  // Only black queen side
  ChessBoard board4("8/8/8/8/8/8/8/8 w q - 0 1");
  EXPECT_FALSE(board4.castlingRights.whiteKingSide);
  EXPECT_FALSE(board4.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board4.castlingRights.blackKingSide);
  EXPECT_TRUE(board4.castlingRights.blackQueenSide);

  // White to move, no en passant, halfmove=5, fullmove=10
  ChessBoard board5("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 5 10");
  EXPECT_TRUE(board5.whiteToMove);
  EXPECT_TRUE(board5.castlingRights.whiteKingSide);
  EXPECT_TRUE(board5.castlingRights.whiteQueenSide);
  EXPECT_FALSE(board5.castlingRights.blackKingSide);
  EXPECT_FALSE(board5.castlingRights.blackQueenSide);
  EXPECT_EQ(board5.enPassantSquare, 0);
  EXPECT_EQ(board5.halfmoveClock, 5);
}

TEST_F(ChessParsingTest, FromAlgebraicQuietMove) {
  // Initial board state for the move e2e4
  const std::string fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard board(fen);

  MoveCTX ctx = fromAlgebraic("e2e4", board);

  EXPECT_EQ(ctx.from, E2);
  EXPECT_EQ(ctx.to, E4);
  EXPECT_EQ(ctx.original, Piece::PAWN);
  EXPECT_EQ(ctx.captured, Piece::NOTHING);
  EXPECT_EQ(ctx.capturedSquare, E4); // For quiet moves, capturedSquare is 'to'
  EXPECT_EQ(ctx.promotion, Piece::NOTHING);
}

TEST_F(ChessParsingTest, FromAlgebraicCaptureMove) {
  // Board state: White pawn on E4, Black Knight on D5
  const std::string fen = "8/8/8/3n4/4P3/8/8/8 w - - 0 1";
  ChessBoard board(fen);

  MoveCTX ctx = fromAlgebraic("e4d5", board);
  EXPECT_EQ(ctx.from, E4);
  EXPECT_EQ(ctx.to, D5);
  EXPECT_EQ(ctx.original, Piece::PAWN);
  EXPECT_EQ(ctx.captured, Piece::KNIGHT); // White pawn captures Black Knight
  EXPECT_EQ(ctx.capturedSquare, D5);
  EXPECT_EQ(ctx.promotion, Piece::NOTHING);
}

TEST_F(ChessParsingTest, FromAlgebraicEnPassantCapture) {
  // Board state: White pawn on E5, Black pawn on D5, en passant target D6
  const std::string fen =
      "8/8/3P4/4P3/8/8/8/8 w - d6 0 1"; // Fen does not accurately represent the
                                        // piece
  // on D5 in this specific FEN, but this test focuses on the en passant logic
  // Let's modify the FEN for accuracy:
  const std::string accurate_fen_for_enpassant =
      "8/8/8/3pP3/8/8/8/8 w - d6 0 1"; // Black pawn on d5, White pawn on e5
  ChessBoard board(accurate_fen_for_enpassant);
  // Explicitly set the enPassantSquare on the board object as the FEN parser
  // might not set it correctly for all edge cases without the fullmove history.
  // However, given the provided fen parsing, the d6 will be parsed correctly.

  // Force piece setup for clarity if FEN parsing is not perfect for this
  // scenario (though the provided FEN should work)
  board.whites.fill(0);
  board.blacks.fill(0);
  board.whites[Piece::PAWN] |= (1ULL << E5);
  board.blacks[Piece::PAWN] |= (1ULL << D5);
  // This is the target square, the captured pawn is on D5
  board.enPassantSquare = D6;

  MoveCTX ctx = fromAlgebraic("e5d6", board);
  EXPECT_EQ(ctx.from, E5);
  EXPECT_EQ(ctx.to, D6);
  EXPECT_EQ(ctx.original, Piece::PAWN);

  // Captured piece should be PAWN for en passant
  EXPECT_EQ(ctx.captured, Piece::PAWN);
  EXPECT_EQ(ctx.capturedSquare, D5); // Captured square is D5, not D6
  EXPECT_EQ(ctx.promotion, Piece::NOTHING);
}

TEST_F(ChessParsingTest, FromAlgebraicPawnPromotionQueen) {
  // Board state: White pawn on G7, moving to G8 to promote
  const std::string fen = "8/6P1/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);

  MoveCTX ctx = fromAlgebraic("g7g8q", board);
  EXPECT_EQ(ctx.from, G7);
  EXPECT_EQ(ctx.to, G8);
  EXPECT_EQ(ctx.original, Piece::PAWN);
  EXPECT_EQ(ctx.captured, Piece::NOTHING);
  EXPECT_EQ(ctx.capturedSquare, G8);
  EXPECT_EQ(ctx.promotion, Piece::QUEEN);
}

TEST_F(ChessParsingTest, FromAlgebraicPawnPromotionKnight) {
  const std::string fen = "8/6P1/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);
  MoveCTX ctx = fromAlgebraic("g7g8n", board);
  EXPECT_EQ(ctx.promotion, Piece::KNIGHT);
}

TEST_F(ChessParsingTest, FromAlgebraicPawnPromotionBishop) {
  const std::string fen = "8/6P1/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);
  MoveCTX ctx = fromAlgebraic("g7g8b", board);
  EXPECT_EQ(ctx.promotion, Piece::BISHOP);
}

TEST_F(ChessParsingTest, FromAlgebraicPawnPromotionRook) {
  const std::string fen = "8/6P1/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);
  MoveCTX ctx = fromAlgebraic("g7g8r", board);
  EXPECT_EQ(ctx.promotion, Piece::ROOK);
}

TEST_F(ChessParsingTest, FromAlgebraicPromotionWithCapture) {
  // Board state: White pawn on G7, Black Rook on H8
  const std::string fen = "7r/6P1/8/8/8/8/8/8 w - - 0 1";
  ChessBoard board(fen);

  MoveCTX ctx = fromAlgebraic("g7h8q", board);
  EXPECT_EQ(ctx.from, G7);
  EXPECT_EQ(ctx.to, H8);
  EXPECT_EQ(ctx.original, Piece::PAWN);
  EXPECT_EQ(ctx.captured, Piece::ROOK); // Captured Black Rook
  EXPECT_EQ(ctx.capturedSquare, H8);
  EXPECT_EQ(ctx.promotion, Piece::QUEEN);
}

TEST_F(ChessParsingTest, FromAlgebraicKingMove) {
  const std::string fen = "8/8/8/8/8/8/8/4K3 w - - 0 1";
  ChessBoard board(fen);

  MoveCTX ctx = fromAlgebraic("e1e2", board);
  EXPECT_EQ(ctx.from, E1);
  EXPECT_EQ(ctx.to, E2);
  EXPECT_EQ(ctx.original, Piece::KING);
  EXPECT_EQ(ctx.captured, Piece::NOTHING);
  EXPECT_EQ(ctx.promotion, Piece::NOTHING);
}

TEST_F(ChessParsingTest, FromAlgebraicRookMove) {
  const std::string fen = "8/8/8/8/8/8/8/R7 w - - 0 1";
  ChessBoard board(fen);

  MoveCTX ctx = fromAlgebraic("a1a8", board);
  EXPECT_EQ(ctx.from, A1);
  EXPECT_EQ(ctx.to, A8);
  EXPECT_EQ(ctx.original, Piece::ROOK);
  EXPECT_EQ(ctx.captured, Piece::NOTHING);
  EXPECT_EQ(ctx.promotion, Piece::NOTHING);
}
