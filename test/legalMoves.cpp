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
    board.halfmoveClock = 0;
    board.enPassantSquare = 0; // No en passant by default
    board.whiteToMove = true;
    board.castlingRights.whiteKingSide = true;
    board.castlingRights.whiteQueenSide = true;
    board.castlingRights.blackKingSide = true;
    board.castlingRights.blackQueenSide = true;

    mg.pseudoLegal.clear();
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

// --- Tests for updateCastlingRights ---
TEST_F(MoveGeneratorTest, UpdateCastlingRightsWhiteKingSideBlocked) {
  // Set up enemy pieces/attacks blocking white's kingside castling
  const std::uint64_t enemyFlat = (1ULL << BoardSquare::F1);
  const std::uint64_t enemyAttacks = 0; // Only flat matters for this test

  updateCastlingRights(board, enemyFlat, true, enemyAttacks);

  EXPECT_FALSE(board.castlingRights.whiteKingSide);
  EXPECT_TRUE(board.castlingRights.whiteQueenSide); // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.blackKingSide);  // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.blackQueenSide); // Should remain unchanged
}

TEST_F(MoveGeneratorTest, UpdateCastlingRightsWhiteQueenSideBlocked) {
  // Set up enemy pieces/attacks blocking white's queenside castling
  const std::uint64_t enemyFlat = (1ULL << BoardSquare::D1);
  const std::uint64_t enemyAttacks = 0;

  updateCastlingRights(board, enemyFlat, true, enemyAttacks);

  EXPECT_TRUE(board.castlingRights.whiteKingSide); // Should remain unchanged
  EXPECT_FALSE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);  // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.blackQueenSide); // Should remain unchanged
}

TEST_F(MoveGeneratorTest, UpdateCastlingRightsBlackKingSideBlocked) {
  // Set up enemy pieces/attacks blocking black's kingside castling
  const std::uint64_t enemyFlat = (1ULL << BoardSquare::F8);
  const std::uint64_t enemyAttacks = 0;

  updateCastlingRights(board, enemyFlat, false, enemyAttacks);

  EXPECT_TRUE(board.castlingRights.whiteKingSide);  // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.whiteQueenSide); // Should remain unchanged
  EXPECT_FALSE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide); // Should remain unchanged
}

TEST_F(MoveGeneratorTest, UpdateCastlingRightsBlackQueenSideBlocked) {
  // Set up enemy pieces/attacks blocking black's queenside castling
  const std::uint64_t enemyFlat = (1ULL << BoardSquare::D8);
  const std::uint64_t enemyAttacks = 0;

  updateCastlingRights(board, enemyFlat, false, enemyAttacks);

  EXPECT_TRUE(board.castlingRights.whiteKingSide);  // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.whiteQueenSide); // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.blackKingSide);  // Should remain unchanged
  EXPECT_FALSE(board.castlingRights.blackQueenSide);
}

TEST_F(MoveGeneratorTest, UpdateCastlingRightsWhiteKingSideAttacked) {
  // Set up enemy attacks (but not pieces) blocking white's kingside castling
  const std::uint64_t enemyFlat = 0;
  const std::uint64_t enemyAttacks = (1ULL << BoardSquare::F1);

  updateCastlingRights(board, enemyFlat, true, enemyAttacks);

  EXPECT_FALSE(board.castlingRights.whiteKingSide);
  EXPECT_TRUE(board.castlingRights.whiteQueenSide); // Should remain unchanged
}

TEST_F(MoveGeneratorTest, UpdateCastlingRightsWhiteBothSidesBlocked) {
  // Set up enemy pieces blocking both white's castling paths
  const std::uint64_t enemyFlat =
      (1ULL << BoardSquare::F1) | (1ULL << BoardSquare::D1);
  const std::uint64_t enemyAttacks = 0;

  updateCastlingRights(board, enemyFlat, true, enemyAttacks);

  EXPECT_FALSE(board.castlingRights.whiteKingSide);
  EXPECT_FALSE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);  // Should remain unchanged
  EXPECT_TRUE(board.castlingRights.blackQueenSide); // Should remain unchanged
}

TEST_F(MoveGeneratorTest, UpdateCastlingRightsNoBlocking) {
  // No blocking pieces or attacks - rights should remain unchanged
  const std::uint64_t enemyFlat = 0;
  const std::uint64_t enemyAttacks = 0;

  updateCastlingRights(board, enemyFlat, true, enemyAttacks);

  EXPECT_TRUE(board.castlingRights.whiteKingSide);
  EXPECT_TRUE(board.castlingRights.whiteQueenSide);
  EXPECT_TRUE(board.castlingRights.blackKingSide);
  EXPECT_TRUE(board.castlingRights.blackQueenSide);
}

// --- Tests for MoveGenerator::appendCastling ---
TEST_F(MoveGeneratorTest, AppendCastlingWhiteBothSidesAvailable) {
  // Set up board with both white castling rights
  board.castlingRights.whiteKingSide = true;
  board.castlingRights.whiteQueenSide = true;

  mg.appendCastling(board, true);

  // Expect both king-side and queen-side castling moves
  ASSERT_EQ(mg.pseudoLegal.size(), 2);

  // King-side castling (E1 to G1)
  MoveCTX expectedKingSide = {.from = BoardSquare::E1,
                              .to = BoardSquare::G1,
                              .original = Piece::KING,
                              .captured = Piece::NOTHING,
                              .promotion = Piece::NOTHING};

  // Queen-side castling (E1 to C1)
  MoveCTX expectedQueenSide = {.from = BoardSquare::E1,
                               .to = BoardSquare::C1,
                               .original = Piece::KING,
                               .captured = Piece::NOTHING,
                               .promotion = Piece::NOTHING};

  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedKingSide));
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedQueenSide));
}

TEST_F(MoveGeneratorTest, AppendCastlingWhiteKingSideOnly) {
  // Set up board with only white king-side castling right
  board.castlingRights.whiteKingSide = true;
  board.castlingRights.whiteQueenSide = false;

  mg.appendCastling(board, true);

  // Expect only king-side castling move
  ASSERT_EQ(mg.pseudoLegal.size(), 1);

  MoveCTX expectedKingSide = {.from = BoardSquare::E1,
                              .to = BoardSquare::G1,
                              .original = Piece::KING,
                              .captured = Piece::NOTHING,
                              .promotion = Piece::NOTHING};

  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedKingSide));
}

TEST_F(MoveGeneratorTest, AppendCastlingWhiteQueenSideOnly) {
  // Set up board with only white queen-side castling right
  board.castlingRights.whiteKingSide = false;
  board.castlingRights.whiteQueenSide = true;

  mg.appendCastling(board, true);

  // Expect only queen-side castling move
  ASSERT_EQ(mg.pseudoLegal.size(), 1);

  MoveCTX expectedQueenSide = {.from = BoardSquare::E1,
                               .to = BoardSquare::C1,
                               .original = Piece::KING,
                               .captured = Piece::NOTHING,
                               .promotion = Piece::NOTHING};

  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedQueenSide));
}

TEST_F(MoveGeneratorTest, AppendCastlingBlackBothSidesAvailable) {
  // Set up board with both black castling rights
  board.castlingRights.blackKingSide = true;
  board.castlingRights.blackQueenSide = true;

  mg.appendCastling(board, false);

  // Expect both king-side and queen-side castling moves
  ASSERT_EQ(mg.pseudoLegal.size(), 2);

  // King-side castling (E8 to G8)
  MoveCTX expectedKingSide = {.from = BoardSquare::E8,
                              .to = BoardSquare::G8,
                              .original = Piece::KING,
                              .captured = Piece::NOTHING,
                              .promotion = Piece::NOTHING};

  // Queen-side castling (E8 to C8)
  MoveCTX expectedQueenSide = {.from = BoardSquare::E8,
                               .to = BoardSquare::C8,
                               .original = Piece::KING,
                               .captured = Piece::NOTHING,
                               .promotion = Piece::NOTHING};

  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedKingSide));
  EXPECT_TRUE(containsMove(mg.pseudoLegal, expectedQueenSide));
}

TEST_F(MoveGeneratorTest, AppendCastlingNoRights) {
  // Set up board with no castling rights
  board.castlingRights.whiteKingSide = false;
  board.castlingRights.whiteQueenSide = false;
  board.castlingRights.blackKingSide = false;
  board.castlingRights.blackQueenSide = false;

  // Test for white
  mg.appendCastling(board, true);
  EXPECT_TRUE(mg.pseudoLegal.empty());

  // Test for black
  mg.pseudoLegal.clear();
  mg.appendCastling(board, false);
  EXPECT_TRUE(mg.pseudoLegal.empty());
}

TEST_F(MoveGeneratorTest, AppendCastlingMixedScenario) {
  // Set up board with white king-side and black queen-side rights only
  board.castlingRights.whiteKingSide = true;
  board.castlingRights.whiteQueenSide = false;
  board.castlingRights.blackKingSide = false;
  board.castlingRights.blackQueenSide = true;

  // Test white
  mg.appendCastling(board, true);
  ASSERT_EQ(mg.pseudoLegal.size(), 1);
  EXPECT_EQ(mg.pseudoLegal[0].to, BoardSquare::G1);

  // Test black
  mg.pseudoLegal.clear();
  mg.appendCastling(board, false);
  ASSERT_EQ(mg.pseudoLegal.size(), 1);
  EXPECT_EQ(mg.pseudoLegal[0].to, BoardSquare::C8);
}
