#include "legalMoves.h"
#include "board.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

// For debugging
void printBitboard(const std::uint64_t bitboard) {
  std::cout << "  A B C D E F G H\n";
  for (int8_t rank = BOARD_LENGTH - 1; rank >= 0; rank--) {
    std::cout << rank + 1 << ' ';
    for (int8_t file = 0; file < BOARD_LENGTH; file++) {
      const bool isBitSet =
          (bitboard & (1ULL << ((rank * BOARD_LENGTH) + file))) != 0;
      std::cout << (isBitSet ? '#' : '.') << ' ';
    }
    std::cout << '\n';
  }
}

// --- GTest Test Fixture for MoveGenerator ---
class MoveGeneratorTest : public ::testing::Test {
protected:
  MoveGenerator mg;
  ChessBoard board;

  void SetUp() override {
    // Initialize a clean board for each test
    board = {}; // Zero-initialize all members
    board.whites.fill(0);
    board.blacks.fill(0);
    board.zobrist = 0;
    board.halfmoveCounter = 0;
    board.enPassantSquare = 0; // No en passant by default
    board.whiteToMove = true;
    board.castlingRights.whiteKingSide = true;
    board.castlingRights.whiteQueenSide = true;
    board.castlingRights.blackKingSide = true;
    board.castlingRights.blackQueenSide = true;

    mg.pseudoLegal.clear();
    mg.legal.clear();
    mg.kills = 0;
  }

  // Helper to set a piece on the board
  void setPiece(Piece piece, BoardSquare square, bool isWhite) {
    if (isWhite) {
      board.whites.at(piece) |= 1ULL << square;
    } else {
      board.blacks.at(piece) |= 1ULL << square;
    }
  }

  // Helper to check if a MoveCTX exists in a vector
  static auto containsMove(const std::vector<MoveCTX> &moves,
                           const MoveCTX &target) -> bool {
    return std::ranges::any_of(moves, [&](const MoveCTX &move) {
      return move.from == target.from && move.to == target.to &&
             move.capturedSquare == target.capturedSquare &&
             move.original == target.original &&
             move.captured == target.captured &&
             move.promotion == target.promotion;
    });
  }
};

// --- Tests for appendContext (indirectly via TestHelpers) ---
TEST_F(MoveGeneratorTest, AppendContextNoCaptureNoPromotion) {
  MoveCTX ctx = {.from = A2, .to = A3, .original = PAWN};
  std::vector<MoveCTX> pseudoLegalMoves;
  std::array<uint64_t, Piece::KING + 1> enemyColor;
  enemyColor.fill(0);
  uint64_t enemyFlat = 0;
  int32_t enPassantSquare = 0;

  appendContext(ctx, true, enemyColor, enemyFlat, pseudoLegalMoves,
                enPassantSquare);

  ASSERT_EQ(pseudoLegalMoves.size(), 1);
  EXPECT_EQ(pseudoLegalMoves[0].from, A2);
  EXPECT_EQ(pseudoLegalMoves[0].to, A3);
  EXPECT_EQ(pseudoLegalMoves[0].original, PAWN);
  EXPECT_EQ(pseudoLegalMoves[0].captured, NOTHING);
  EXPECT_EQ(pseudoLegalMoves[0].promotion, NOTHING);
  EXPECT_EQ(pseudoLegalMoves[0].capturedSquare, A3);
}

TEST_F(MoveGeneratorTest, AppendContextCaptureNoPromotion) {
  MoveCTX ctx = {.from = A2, .to = B3, .original = PAWN};
  std::vector<MoveCTX> pseudoLegalMoves;
  std::array<uint64_t, Piece::KING + 1> enemyColor;
  enemyColor.fill(0);
  enemyColor.at(KNIGHT) |= (1ULL << B3); // Enemy Knight on B3
  uint64_t enemyFlat = (1ULL << B3);
  int32_t enPassantSquare = 0;

  appendContext(ctx, true, enemyColor, enemyFlat, pseudoLegalMoves,
                enPassantSquare);

  ASSERT_EQ(pseudoLegalMoves.size(), 1);
  EXPECT_EQ(pseudoLegalMoves[0].from, A2);
  EXPECT_EQ(pseudoLegalMoves[0].to, B3);
  EXPECT_EQ(pseudoLegalMoves[0].original, PAWN);
  EXPECT_EQ(pseudoLegalMoves[0].captured, KNIGHT);
  EXPECT_EQ(pseudoLegalMoves[0].promotion, NOTHING);
  EXPECT_EQ(pseudoLegalMoves[0].capturedSquare, B3);
}

TEST_F(MoveGeneratorTest, AppendContextEnPassantCapture) {
  MoveCTX ctx = {
      .from = E5, .to = D6, .original = PAWN}; // White pawn moves E5 to D6
  std::vector<MoveCTX> pseudoLegalMoves;
  std::array<uint64_t, Piece::KING + 1> enemyColor;
  enemyColor.fill(0);
  enemyColor.at(PAWN) |= (1ULL << D5); // Black pawn on D5 (captured square)
  uint64_t enemyFlat = (1ULL << D5);
  int32_t enPassantSquare = D6; // En passant target square

  appendContext(ctx, true, enemyColor, enemyFlat, pseudoLegalMoves,
                enPassantSquare);

  ASSERT_EQ(pseudoLegalMoves.size(), 1);
  EXPECT_EQ(pseudoLegalMoves[0].from, E5);
  EXPECT_EQ(pseudoLegalMoves[0].to, D6);
  EXPECT_EQ(pseudoLegalMoves[0].original, PAWN);
  // Captured is PAWN because it's en passant
  EXPECT_EQ(pseudoLegalMoves[0].captured, Piece::PAWN);
  EXPECT_EQ(pseudoLegalMoves[0].promotion, NOTHING);
  // Captured square is D5 for en passant
  EXPECT_EQ(pseudoLegalMoves[0].capturedSquare, D5);
}

TEST_F(MoveGeneratorTest, AppendContextPawnPromotionWhite) {
  MoveCTX ctx = {
      .from = A7,
      .to = A8,
      .capturedSquare = A8,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = NOTHING,
  }; // White pawn moves A7 to A8

  std::vector<MoveCTX> pseudoLegalMoves;
  std::array<uint64_t, Piece::KING + 1> enemyColor;

  uint64_t enemyFlat = 0;
  int32_t enPassantSquare = 0;

  appendContext(ctx, true, enemyColor, enemyFlat, pseudoLegalMoves,
                enPassantSquare);

  // Queen, Rook, Bishop, Knight promotions
  ASSERT_EQ(pseudoLegalMoves.size(), 4);
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = A7,
                                              .to = A8,
                                              .capturedSquare = A8,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = KNIGHT}));
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = A7,
                                              .to = A8,
                                              .capturedSquare = A8,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = BISHOP}));
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = A7,
                                              .to = A8,
                                              .capturedSquare = A8,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = ROOK}));
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = A7,
                                              .to = A8,
                                              .capturedSquare = A8,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = QUEEN}));
}

TEST_F(MoveGeneratorTest, AppendContextPawnPromotionBlack) {
  MoveCTX ctx = {
      .from = H2, .to = H1, .original = PAWN}; // Black pawn moves H2 to H1
  std::vector<MoveCTX> pseudoLegalMoves;
  std::array<uint64_t, Piece::KING + 1> enemyColor;
  enemyColor.fill(0);
  uint64_t enemyFlat = 0;
  int32_t enPassantSquare = 0;

  appendContext(ctx, false, enemyColor, enemyFlat, pseudoLegalMoves,
                enPassantSquare);

  // Queen, Rook, Bishop, Knight promotions
  ASSERT_EQ(pseudoLegalMoves.size(), 4);
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = H2,
                                              .to = H1,
                                              .capturedSquare = H1,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = KNIGHT}));
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = H2,
                                              .to = H1,
                                              .capturedSquare = H1,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = BISHOP}));
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = H2,
                                              .to = H1,
                                              .capturedSquare = H1,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = ROOK}));
  EXPECT_TRUE(containsMove(pseudoLegalMoves, {.from = H2,
                                              .to = H1,
                                              .capturedSquare = H1,
                                              .original = PAWN,
                                              .captured = NOTHING,
                                              .promotion = QUEEN}));
}

// --- Tests for MoveGenerator::generatePseudoLegal ---
TEST_F(MoveGeneratorTest, GeneratePseudoLegalInitialBoardWhitePawns) {
  // Set up an initial board state for white pawns
  setPiece(PAWN, A2, true);
  setPiece(PAWN, B2, true);
  setPiece(PAWN, C2, true);
  setPiece(PAWN, D2, true);
  setPiece(PAWN, E2, true);
  setPiece(PAWN, F2, true);
  setPiece(PAWN, G2, true);
  setPiece(PAWN, H2, true);
  setPiece(KNIGHT, B3, false);

  mg.generatePseudoLegal(board, true);

  // Expect 2 moves per pawn (1 or 2 squares forward) = 16 moves
  // Plus potential captures if mock getPseudoLegal generates them
  // The mock getPseudoLegal for pawns generates 1 or 2 forward moves + 2
  // diagonal captures. Since there are no enemy pieces, captures won't result
  // in captured pieces, but the pseudoLegalMoves will still contain them. Total
  // moves: 8 pawns * (2 quiet + 2 kills) = 32 moves (some might be duplicates
  // or invalid on real board) Let's refine based on the mock: A2: A3, A4, B3
  // (kill), nothing (kill) -> 4 moves H2: H3, H4, G3 (kill), nothing (kill) ->
  // 4 moves B2-G2: 6 * (A3, A4, A1, A3) -> 4 moves each This mock is very
  // basic. Let's test for a specific, simple pawn move.
  ASSERT_GT(mg.pseudoLegal.size(), 0); // Should generate some moves

  // Check for a specific pawn move (e.g., A2 to A3)
  MoveCTX expectedMoveA2A3 = {
      .from = A2,
      .to = A3,
      .capturedSquare = A3,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = NOTHING,
  };
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedMoveA2A3));

  // Check for A2 to A4
  MoveCTX expectedMoveA2A4 = {
      .from = A2,
      .to = A4,
      .capturedSquare = A4,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = NOTHING,
  };
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedMoveA2A4));

  // Check that kills bitboard is updated (e.g., B3 should be in kills from A2)
  EXPECT_TRUE((mg.kills & (1ULL << B3)) != 0);
}

TEST_F(MoveGeneratorTest, GeneratePseudoLegalPawnCapture) {
  setPiece(PAWN, E2, true);  // White pawn
  setPiece(PAWN, D3, false); // Black pawn on D3
  board.whiteToMove = true;

  mg.generatePseudoLegal(board, true);

  // Expected capture move: E2 to D3, capturing black pawn
  MoveCTX expectedCapture = {
      .from = E2,
      .to = D3,
      .capturedSquare = D3,
      .original = PAWN,
      .captured = PAWN,
      .promotion = NOTHING,
  };
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedCapture));
  EXPECT_TRUE((mg.kills & (1ULL << D3)) != 0); // D3 should be in kills
}

TEST_F(MoveGeneratorTest, GeneratePseudoLegalEnPassantScenario) {
  // Setup: White pawn on E5, black pawn on D7 moved to D5 (just moved, so en
  // passant is possible)
  setPiece(PAWN, E5, true);   // White pawn
  setPiece(PAWN, D5, false);  // Black pawn
  board.enPassantSquare = D6; // En passant target square for white (D5 to D6)
  board.whiteToMove = true;

  mg.generatePseudoLegal(board, true);

  // Expected en passant move: E5 to D6, capturing pawn on D5
  MoveCTX expectedEnPassant = {
      .from = E5,
      .to = D6,
      .capturedSquare = D5,
      .original = PAWN,
      .captured = PAWN,
      .promotion = NOTHING,
  };

  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedEnPassant));
}

TEST_F(MoveGeneratorTest, GeneratePseudoLegalPawnPromotion) {
  setPiece(PAWN, G7, true); // White pawn on 7th rank
  board.whiteToMove = true;

  mg.generatePseudoLegal(board, true);

  // Expect 4 promotion moves for G7 to G8
  MoveCTX expectedQueenPromo = {
      .from = G7,
      .to = G8,
      .capturedSquare = G8,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = QUEEN,
  };
  MoveCTX expectedRookPromo = {
      .from = G7,
      .to = G8,
      .capturedSquare = G8,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = ROOK,
  };
  MoveCTX expectedBishopPromo = {
      .from = G7,
      .to = G8,
      .capturedSquare = G8,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = BISHOP,
  };
  MoveCTX expectedKnightPromo = {
      .from = G7,
      .to = G8,
      .capturedSquare = G8,
      .original = PAWN,
      .captured = NOTHING,
      .promotion = KNIGHT,
  };

  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedQueenPromo));
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedRookPromo));
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedBishopPromo));
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedKnightPromo));
}
